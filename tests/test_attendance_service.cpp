#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "attendance/attendance_service.h"
#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "course/course_service.h"
#include "student/student_profiles.h"

using srs::auth::AuthenticationService;
using srs::auth::Role;
using srs::auth::User;
using srs::auth::UserAccountService;
using srs::core::ErrorCode;
using srs::course::Course;
using srs::course::CourseService;
using srs::attendance::AttendanceService;
using srs::attendance::AttendanceStatus;
using srs::student::StudentProfile;
using srs::student::StudentProfileService;

namespace {

struct AttendanceFixture {
    AuthenticationService auth{std::vector<User>{
        {"admin01", "Admin1234", Role::Admin},
        {"faculty01", "Teach1234", Role::Faculty},
        {"faculty02", "Teach5678", Role::Faculty},
        {"student01", "Pass1234", Role::Student},
        {"student02", "Pass5678", Role::Student}
    }};

    UserAccountService accounts{auth};
    StudentProfileService students{auth};
    CourseService courses{accounts, auth, students};
    AttendanceService attendance{accounts, auth, students, courses};

    std::string admin;
    std::string faculty1;
    std::string faculty2;
    std::string student1;
    std::string student2;

    AttendanceFixture() {
        admin = login("admin01", "Admin1234");
        REQUIRE(students.register_student(
            admin,
            StudentProfile{
                "student01", "Alice Smith", "SRN-001",
                "2005-01-01", "555-0100",
                "Computer Science", 3, true
            }));

        REQUIRE(students.register_student(
            admin,
            StudentProfile{
                "student02", "Bob Jones", "SRN-002",
                "2005-02-02", "555-0101",
                "Computer Science", 3, true
            }));

        faculty1 = login("faculty01", "Teach1234");
        faculty2 = login("faculty02", "Teach5678");
        student1 = login("student01", "Pass1234");
        student2 = login("student02", "Pass5678");

        REQUIRE(courses.create_course(
            admin, Course{"CS201", "Data Structures", 4, "faculty01"}));

        REQUIRE(courses.enroll_student(
            admin, "SRN-001", "CS201", 3));

        REQUIRE(courses.enroll_student(
            admin, "SRN-002", "CS201", 3));
    }

    std::string login(
        const std::string& username,
        const std::string& password) {

        const auto session = auth.login(username, password);
        REQUIRE(session != nullptr);
        return session->id;
    }

    void mark(
        const std::string& srn,
        const std::string& date,
        const std::string& session_details,
        AttendanceStatus status) {

        REQUIRE(attendance.mark_attendance(
            faculty1,
            srn,
            "CS201",
            3,
            date,
            session_details,
            status));
    }
};

}  // namespace

// F-017: Faculty can record all three attendance statuses.
TEST_CASE("Faculty can mark Present Absent and Late", "[attendance][F-017]") {
    AttendanceFixture f;

    f.mark("SRN-001", "2026-11-01", "Morning Session", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-02", "Morning Session", AttendanceStatus::Absent);
    f.mark("SRN-001", "2026-11-03", "Morning Session", AttendanceStatus::Late);

    const auto records =
        f.attendance.view_my_attendance(f.student1, "CS201");

    REQUIRE(records);
    REQUIRE(records.value()->size() == 3);
}

// F-017: Attendance can be corrected for the same session.
TEST_CASE("Re-entering a session updates its attendance", "[attendance][F-017]") {
    AttendanceFixture f;

    f.mark("SRN-001", "2026-11-01", "Morning Session", AttendanceStatus::Absent);
    f.mark("SRN-001", "2026-11-01", "Morning Session", AttendanceStatus::Present);

    const auto records =
        f.attendance.view_my_attendance(f.student1, "CS201");

    REQUIRE(records);
    REQUIRE(records.value()->size() == 1);
    REQUIRE(records.value()->front().status == AttendanceStatus::Present);
}

// F-017: Only the assigned faculty can mark attendance.
TEST_CASE("Only assigned faculty can mark attendance", "[attendance][F-017][rbac]") {
    AttendanceFixture f;

    auto result = f.attendance.mark_attendance(
        f.faculty2, "SRN-001", "CS201", 3,
        "2026-11-01", "Morning Session", AttendanceStatus::Present);

    REQUIRE_FALSE(result);
    REQUIRE(result.error() == ErrorCode::PERMISSION_DENIED);

    result = f.attendance.mark_attendance(
        f.student1, "SRN-001", "CS201", 3,
        "2026-11-01", "Morning Session", AttendanceStatus::Present);

    REQUIRE_FALSE(result);
    REQUIRE(result.error() == ErrorCode::PERMISSION_DENIED);

    result = f.attendance.mark_attendance(
        "forged-session", "SRN-001", "CS201", 3,
        "2026-11-01", "Morning Session", AttendanceStatus::Present);

    REQUIRE_FALSE(result);
    REQUIRE(result.error() == ErrorCode::SESSION_ERROR);
}

// F-017: Invalid input is rejected.
TEST_CASE("Invalid attendance input is rejected", "[attendance][F-017]") {
    AttendanceFixture f;

    REQUIRE(f.attendance.mark_attendance(
        f.faculty1, "SRN-001", "CS201", 3,
        "2026-02-30", "Morning Session", AttendanceStatus::Present
    ).error() == ErrorCode::INVALID_INPUT);

    REQUIRE(f.attendance.mark_attendance(
        f.faculty1, "SRN-001", "CS201", 3,
        "2026-11-01", "   ", AttendanceStatus::Present
    ).error() == ErrorCode::INVALID_INPUT);

    REQUIRE(f.attendance.mark_attendance(
        f.faculty1, "SRN-001", "CS201", 3,
        "2026-11-01", "Morning Session",
        static_cast<AttendanceStatus>(99)
    ).error() == ErrorCode::INVALID_INPUT);

    REQUIRE(f.attendance.mark_attendance(
        f.faculty1, "SRN-001", "CS201", 13,
        "2026-11-01", "Morning Session", AttendanceStatus::Present
    ).error() == ErrorCode::INVALID_INPUT);
}

// F-018: Percentage is calculated and rounded to two decimal places.
TEST_CASE("Attendance percentage is rounded to two decimals", "[attendance][F-018]") {
    AttendanceFixture f;

    f.mark("SRN-001", "2026-11-01", "Session 1", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-02", "Session 2", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-03", "Session 3", AttendanceStatus::Absent);

    const auto summary =
        f.attendance.view_my_summary(f.student1, "CS201", 3);

    REQUIRE(summary);
    REQUIRE(summary.value()->total_sessions == 3);
    REQUIRE(summary.value()->present_sessions == 2);
    REQUIRE(summary.value()->percentage == 66.67);
}

// Late is included in total sessions but not in present sessions.
TEST_CASE("Late attendance is included in total but not present count", "[attendance][F-018]") {
    AttendanceFixture f;

    f.mark("SRN-001", "2026-11-01", "Session 1", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-02", "Session 2", AttendanceStatus::Late);
    f.mark("SRN-001", "2026-11-03", "Session 3", AttendanceStatus::Absent);

    const auto summary =
        f.attendance.view_my_summary(f.student1, "CS201", 3);

    REQUIRE(summary);
    REQUIRE(summary.value()->total_sessions == 3);
    REQUIRE(summary.value()->present_sessions == 1);
    REQUIRE(summary.value()->percentage == 33.33);
}

// F-019: Default threshold is 75%.
TEST_CASE("Attendance below default threshold is flagged", "[attendance][F-019]") {
    AttendanceFixture f;

    f.mark("SRN-001", "2026-11-01", "Session 1", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-02", "Session 2", AttendanceStatus::Absent);

    const auto summary =
        f.attendance.view_my_summary(f.student1, "CS201", 3);

    REQUIRE(summary);
    REQUIRE(summary.value()->percentage == 50.0);
    REQUIRE(summary.value()->below_threshold);
}

// F-019: Exactly 75% is not below the threshold.
TEST_CASE("Attendance exactly at threshold is not flagged", "[attendance][F-019]") {
    AttendanceFixture f;

    f.mark("SRN-001", "2026-11-01", "Session 1", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-02", "Session 2", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-03", "Session 3", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-04", "Session 4", AttendanceStatus::Absent);

    const auto summary =
        f.attendance.view_my_summary(f.student1, "CS201", 3);

    REQUIRE(summary);
    REQUIRE(summary.value()->percentage == 75.0);
    REQUIRE_FALSE(summary.value()->below_threshold);
}

// F-019: Admin can configure the threshold.
TEST_CASE("Admin can change attendance threshold", "[attendance][F-019]") {
    AttendanceFixture f;

    REQUIRE(f.attendance.set_threshold(f.admin, 80.0));

    f.mark("SRN-001", "2026-11-01", "Session 1", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-02", "Session 2", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-03", "Session 3", AttendanceStatus::Present);
    f.mark("SRN-001", "2026-11-04", "Session 4", AttendanceStatus::Absent);

    const auto summary =
        f.attendance.view_my_summary(f.student1, "CS201", 3);

    REQUIRE(summary);
    REQUIRE(summary.value()->percentage == 75.0);
    REQUIRE(summary.value()->below_threshold);
}

// F-019: Only admins can configure the threshold.
TEST_CASE("Only admin can configure attendance threshold", "[attendance][F-019][rbac]") {
    AttendanceFixture f;

    REQUIRE(f.attendance.set_threshold(
        f.student1, 80.0).error() == ErrorCode::PERMISSION_DENIED);

    REQUIRE(f.attendance.set_threshold(
        f.faculty1, 80.0).error() == ErrorCode::PERMISSION_DENIED);

    REQUIRE(f.attendance.set_threshold(
        "forged-session", 80.0).error() == ErrorCode::SESSION_ERROR);

    REQUIRE(f.attendance.set_threshold(
        f.admin, -1.0).error() == ErrorCode::INVALID_INPUT);

    REQUIRE(f.attendance.set_threshold(
        f.admin, 101.0).error() == ErrorCode::INVALID_INPUT);
}

// F-020: Students can view only their own records.
TEST_CASE("Student can view only their own attendance", "[attendance][F-020]") {
    AttendanceFixture f;

    f.mark("SRN-001", "2026-11-01", "Session 1", AttendanceStatus::Present);

    const auto alice =
        f.attendance.view_my_attendance(f.student1, "CS201");

    REQUIRE(alice);
    REQUIRE(alice.value()->size() == 1);
    REQUIRE(alice.value()->front().srn == "SRN-001");

    const auto bob =
        f.attendance.view_my_attendance(f.student2, "CS201");

    REQUIRE(bob);
    REQUIRE(bob.value()->empty());
}

// F-020: Non-students cannot use student attendance viewing.
TEST_CASE("Faculty cannot view own student attendance", "[attendance][F-020][rbac]") {
    AttendanceFixture f;

    const auto result =
        f.attendance.view_my_attendance(f.faculty1, "CS201");

    REQUIRE_FALSE(result);
    REQUIRE(result.error() == ErrorCode::PERMISSION_DENIED);
}

// F-020: Invalid course input is rejected.
TEST_CASE("Invalid attendance viewing requests are rejected", "[attendance][F-020]") {
    AttendanceFixture f;

    REQUIRE(f.attendance.view_my_attendance(
        f.student1, "").error() == ErrorCode::INVALID_INPUT);

    REQUIRE(f.attendance.view_my_attendance(
        f.student1, "UNKNOWN").error() == ErrorCode::NOT_FOUND);

    REQUIRE(f.attendance.view_my_summary(
        f.student1, "CS201", 13).error() == ErrorCode::INVALID_INPUT);
}