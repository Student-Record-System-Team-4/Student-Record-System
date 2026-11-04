#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>
#include <vector>

#include "attendance/attendance_service.h"
#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "course/course_service.h"
#include "persistence/database.h"
#include "student/student_profiles.h"

// SRS-F-017..F-020: attendance data must survive a restart. These tests drive
// AttendanceService itself and "restart" by destroying every service and
// reopening the same database file.

using srs::attendance::AttendanceService;
using srs::attendance::AttendanceStatus;
using srs::auth::AuthenticationService;
using srs::auth::Role;
using srs::auth::User;
using srs::auth::UserAccountService;
using srs::core::ErrorCode;
using srs::course::Course;
using srs::course::CourseService;
using srs::persistence::Database;
using srs::student::StudentProfile;
using srs::student::StudentProfileService;

namespace {

const char* test_database = "srs-attendance-service-persistence-test.db";

std::vector<User> seed_users() {
    return {
        {"admin01", "Admin1234", Role::Admin},
        {"faculty01", "Teach1234", Role::Faculty},
        {"faculty02", "Teach5678", Role::Faculty},
        {"student01", "Pass1234", Role::Student},
        {"student02", "Pass5678", Role::Student}};
}

// One "process lifetime": every service wired to the same open database.
struct App {
    Database database;
    AuthenticationService auth;
    UserAccountService accounts;
    StudentProfileService students;
    CourseService courses;
    AttendanceService attendance;

    explicit App(const char* path)
        : database(path),
          auth(seed_users(), database),
          accounts(auth),
          students(auth, database),
          courses(accounts, auth, students, database),
          attendance(accounts, auth, students, courses, database) {
        REQUIRE(database.is_open());
    }

    std::string login(const std::string& user, const std::string& password) {
        const auto session = auth.login(user, password);
        REQUIRE(session != nullptr);
        return session->id;
    }

    void mark(
        const std::string& faculty_session,
        const std::string& srn,
        const std::string& date,
        const std::string& details,
        AttendanceStatus status) {
        REQUIRE(attendance.mark_attendance(
            faculty_session, srn, "CS201", 3, date, details, status));
    }
};

// faculty01 teaches CS201; Alice (SRN-001) and Bob (SRN-002) are enrolled in
// it in semester 3.
void seed_academic_data(App& app) {
    const auto admin = app.login("admin01", "Admin1234");

    REQUIRE(app.students.register_student(
        admin,
        StudentProfile{"student01", "Alice Smith", "SRN-001", "2005-01-01",
                       "555-0100", "Computer Science", 3, true}));
    REQUIRE(app.students.register_student(
        admin,
        StudentProfile{"student02", "Bob Jones", "SRN-002", "2005-02-02",
                       "555-0101", "Computer Science", 3, true}));

    REQUIRE(app.courses.create_course(
        admin, Course{"CS201", "Data Structures", 4, "faculty01"}));
    REQUIRE(app.courses.enroll_student(admin, "SRN-001", "CS201", 3));
    REQUIRE(app.courses.enroll_student(admin, "SRN-002", "CS201", 3));
}

}  // namespace

TEST_CASE("Attendance and percentages survive a restart",
          "[attendance][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        seed_academic_data(app);
        const auto faculty = app.login("faculty01", "Teach1234");

        app.mark(faculty, "SRN-001", "2026-11-01", "Session 1", AttendanceStatus::Present);
        app.mark(faculty, "SRN-001", "2026-11-02", "Session 2", AttendanceStatus::Present);
        app.mark(faculty, "SRN-001", "2026-11-03", "Session 3", AttendanceStatus::Late);
        app.mark(faculty, "SRN-001", "2026-11-04", "Session 4", AttendanceStatus::Absent);
        app.mark(faculty, "SRN-002", "2026-11-01", "Session 1", AttendanceStatus::Absent);
    }

    {
        App app(test_database);
        const auto alice = app.login("student01", "Pass1234");
        const auto bob = app.login("student02", "Pass5678");

        const auto records = app.attendance.view_my_attendance(alice, "CS201");
        REQUIRE(records);
        REQUIRE(records.value()->size() == 4);

        // Every record belongs to Alice, with the stored status.
        int present = 0;
        int late = 0;
        int absent = 0;
        for (const auto& record : *records.value()) {
            REQUIRE(record.srn == "SRN-001");
            REQUIRE(record.course_code == "CS201");
            REQUIRE(record.semester == 3);
            present += record.status == AttendanceStatus::Present;
            late += record.status == AttendanceStatus::Late;
            absent += record.status == AttendanceStatus::Absent;
        }
        REQUIRE(present == 2);
        REQUIRE(late == 1);
        REQUIRE(absent == 1);

        // 2 present out of 4 sessions (Late counts as a session, not present).
        const auto summary = app.attendance.view_my_summary(alice, "CS201", 3);
        REQUIRE(summary);
        REQUIRE(summary.value()->total_sessions == 4);
        REQUIRE(summary.value()->present_sessions == 2);
        REQUIRE(summary.value()->percentage == 50.0);
        REQUIRE(summary.value()->below_threshold);

        // Students still only see their own records.
        const auto bobs = app.attendance.view_my_attendance(bob, "CS201");
        REQUIRE(bobs);
        REQUIRE(bobs.value()->size() == 1);
        REQUIRE(bobs.value()->front().srn == "SRN-002");
    }

    std::remove(test_database);
}

TEST_CASE("Corrections persist and never create a second entry",
          "[attendance][persistence][duplicates]") {
    std::remove(test_database);

    {
        App app(test_database);
        seed_academic_data(app);
        const auto faculty = app.login("faculty01", "Teach1234");

        app.mark(faculty, "SRN-001", "2026-11-01", "Lecture 1", AttendanceStatus::Absent);
        app.mark(faculty, "SRN-001", "2026-11-02", "Lecture 2", AttendanceStatus::Present);
    }

    {
        App app(test_database);
        const auto faculty = app.login("faculty01", "Teach1234");
        const auto alice = app.login("student01", "Pass1234");

        REQUIRE(app.attendance.view_my_summary(alice, "CS201", 3)
                    .value()->percentage == 50.0);

        // Correct the absence after the restart, typing the session
        // differently; this must update the stored entry, not add one.
        app.mark(faculty, "SRN-001", "2026-11-01", " lecture 1 ", AttendanceStatus::Present);
    }

    {
        App app(test_database);
        const auto alice = app.login("student01", "Pass1234");

        const auto records = app.attendance.view_my_attendance(alice, "CS201");
        REQUIRE(records);
        REQUIRE(records.value()->size() == 2);  // corrected, not duplicated

        const auto summary = app.attendance.view_my_summary(alice, "CS201", 3);
        REQUIRE(summary.value()->total_sessions == 2);
        REQUIRE(summary.value()->present_sessions == 2);
        REQUIRE(summary.value()->percentage == 100.0);
        REQUIRE_FALSE(summary.value()->below_threshold);

        bool found = false;
        for (const auto& record : *records.value()) {
            if (record.date == "2026-11-01") {
                found = true;
                REQUIRE(record.session_details == "Lecture 1");
                REQUIRE(record.status == AttendanceStatus::Present);
            }
        }
        REQUIRE(found);
    }

    std::remove(test_database);
}

TEST_CASE("Rejected attendance entries leave nothing behind after a restart",
          "[attendance][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        seed_academic_data(app);

        const auto faculty1 = app.login("faculty01", "Teach1234");
        const auto faculty2 = app.login("faculty02", "Teach5678");
        const auto alice = app.login("student01", "Pass1234");
        const auto admin = app.login("admin01", "Admin1234");

        const auto mark = [&](const std::string& session,
                              const std::string& srn,
                              const std::string& course,
                              int semester,
                              const std::string& date,
                              const std::string& details) {
            return app.attendance.mark_attendance(
                session, srn, course, semester, date, details,
                AttendanceStatus::Present);
        };

        // Invalid input.
        REQUIRE_FALSE(mark(faculty1, "SRN-001", "CS201", 3, "2026-02-30", "S1"));
        REQUIRE_FALSE(mark(faculty1, "SRN-001", "CS201", 3, "01-11-2026", "S1"));
        REQUIRE_FALSE(mark(faculty1, "SRN-001", "CS201", 3, "2026-11-01", "  "));
        REQUIRE_FALSE(mark(faculty1, "SRN-001", "CS201", 3, "2026-11-01", "A\nB"));

        // Not enrolled / unknown references.
        REQUIRE_FALSE(mark(faculty1, "SRN-001", "CS201", 4, "2026-11-01", "S1"));
        REQUIRE_FALSE(mark(faculty1, "SRN-999", "CS201", 3, "2026-11-01", "S1"));
        REQUIRE_FALSE(mark(faculty1, "SRN-001", "NOPE", 3, "2026-11-01", "S1"));

        // Wrong faculty, and roles that may not mark attendance at all.
        REQUIRE_FALSE(mark(faculty2, "SRN-001", "CS201", 3, "2026-11-01", "S1"));
        REQUIRE_FALSE(mark(alice, "SRN-001", "CS201", 3, "2026-11-01", "S1"));
        REQUIRE_FALSE(mark(admin, "SRN-001", "CS201", 3, "2026-11-01", "S1"));
    }

    {
        App app(test_database);
        const auto alice = app.login("student01", "Pass1234");
        const auto bob = app.login("student02", "Pass5678");

        REQUIRE(app.attendance.view_my_attendance(alice, "CS201").value()->empty());
        REQUIRE(app.attendance.view_my_attendance(bob, "CS201").value()->empty());

        const auto summary = app.attendance.view_my_summary(alice, "CS201", 3);
        REQUIRE(summary);
        REQUIRE(summary.value()->total_sessions == 0);
        REQUIRE(summary.value()->percentage == 0.0);
    }

    std::remove(test_database);
}
