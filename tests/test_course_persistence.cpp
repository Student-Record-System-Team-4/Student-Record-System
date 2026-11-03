#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>
#include <vector>

#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "course/course_service.h"
#include "persistence/database.h"
#include "student/student_profiles.h"

// SRS-F-010 / F-011 / F-012: course and enrollment data must survive a
// restart. These tests drive CourseService itself (not the repositories) and
// "restart" by destroying every service and reopening the same database file.

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

const char* test_database = "srs-course-service-persistence-test.db";

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

    explicit App(const char* path)
        : database(path),
          auth(seed_users(), database),
          accounts(auth),
          students(auth, database),
          courses(accounts, auth, students, database) {
        REQUIRE(database.is_open());
    }

    std::string login(const std::string& user, const std::string& password) {
        const auto session = auth.login(user, password);
        REQUIRE(session != nullptr);
        return session->id;
    }
};

void register_students(App& app, const std::string& admin) {
    REQUIRE(app.students.register_student(
        admin,
        StudentProfile{"student01", "Alice Smith", "SRN-001", "2005-01-01",
                       "555-0100", "Computer Science", 3, true}));
    REQUIRE(app.students.register_student(
        admin,
        StudentProfile{"student02", "Bob Jones", "SRN-002", "2005-02-02",
                       "555-0101", "Computer Science", 3, true}));
}

}  // namespace

TEST_CASE("Courses and enrollments survive a restart",
          "[course][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        const auto admin = app.login("admin01", "Admin1234");
        register_students(app, admin);

        REQUIRE(app.courses.create_course(
            admin, Course{"CS201", "Data Structures", 4, "faculty01"}));
        REQUIRE(app.courses.create_course(
            admin, Course{"CS202", "Algorithms", 3, "faculty01"}));

        REQUIRE(app.courses.enroll_student(admin, "SRN-001", "CS201", 3));
        REQUIRE(app.courses.enroll_student(admin, "SRN-002", "CS201", 3));
    }

    {
        App app(test_database);
        const auto admin = app.login("admin01", "Admin1234");

        const auto cs201 = app.courses.get_course("CS201");
        REQUIRE(cs201);
        REQUIRE(cs201.value()->title == "Data Structures");
        REQUIRE(cs201.value()->credits == 4);
        REQUIRE(cs201.value()->faculty_username == "faculty01");
        REQUIRE(cs201.value()->active);

        REQUIRE(app.courses.list_courses(admin).value()->size() == 2);
        REQUIRE(app.courses.is_enrolled("SRN-001", "CS201", 3));
        REQUIRE(app.courses.is_enrolled("SRN-002", "CS201", 3));

        const auto roster = app.courses.view_roster(admin, "CS201");
        REQUIRE(roster);
        REQUIRE(roster.value()->size() == 2);
        REQUIRE(roster.value()->at(0).srn == "SRN-001");
        REQUIRE(roster.value()->at(0).name == "Alice Smith");
        REQUIRE(roster.value()->at(1).srn == "SRN-002");
    }

    std::remove(test_database);
}

TEST_CASE("Duplicate enrollment is still rejected after a restart",
          "[enrolment][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        const auto admin = app.login("admin01", "Admin1234");
        register_students(app, admin);
        REQUIRE(app.courses.create_course(
            admin, Course{"CS201", "Data Structures", 4, "faculty01"}));
        REQUIRE(app.courses.enroll_student(admin, "SRN-001", "CS201", 3));
    }

    {
        App app(test_database);
        const auto admin = app.login("admin01", "Admin1234");

        const auto duplicate =
            app.courses.enroll_student(admin, "SRN-001", "CS201", 3);
        REQUIRE_FALSE(duplicate);
        REQUIRE(duplicate.error() == ErrorCode::ALREADY_EXISTS);

        REQUIRE(app.courses.view_roster(admin, "CS201").value()->size() == 1);
    }

    std::remove(test_database);
}

TEST_CASE("Course updates, deactivation and removals persist",
          "[course][enrolment][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        const auto admin = app.login("admin01", "Admin1234");
        register_students(app, admin);

        REQUIRE(app.courses.create_course(
            admin, Course{"CS201", "Data Structures", 4, "faculty01"}));
        REQUIRE(app.courses.create_course(
            admin, Course{"CS202", "Algorithms", 3, "faculty01"}));

        REQUIRE(app.courses.enroll_student(admin, "SRN-001", "CS201", 3));
        REQUIRE(app.courses.enroll_student(admin, "SRN-002", "CS201", 3));
        REQUIRE(app.courses.enroll_student(admin, "SRN-001", "CS202", 3));

        // Modify CS201: new title, credits and faculty.
        REQUIRE(app.courses.update_course(
            admin,
            Course{"CS201", "Advanced Data Structures", 5, "faculty02"}));

        // Deactivate CS202; remove SRN-002 from CS201.
        REQUIRE(app.courses.deactivate_course(admin, "CS202"));
        REQUIRE(app.courses.remove_student(admin, "SRN-002", "CS201", 3));
    }

    {
        App app(test_database);
        const auto admin = app.login("admin01", "Admin1234");
        const auto faculty02 = app.login("faculty02", "Teach5678");

        const auto cs201 = app.courses.get_course("CS201");
        REQUIRE(cs201);
        REQUIRE(cs201.value()->title == "Advanced Data Structures");
        REQUIRE(cs201.value()->credits == 5);
        REQUIRE(cs201.value()->faculty_username == "faculty02");
        REQUIRE(cs201.value()->active);

        const auto cs202 = app.courses.get_course("CS202");
        REQUIRE(cs202);
        REQUIRE_FALSE(cs202.value()->active);

        // The removal persisted; the other enrollments are intact.
        REQUIRE_FALSE(app.courses.is_enrolled("SRN-002", "CS201", 3));
        REQUIRE(app.courses.is_enrolled("SRN-001", "CS201", 3));
        REQUIRE(app.courses.is_enrolled("SRN-001", "CS202", 3));

        // Reassigned faculty can now see the roster; the old one cannot.
        REQUIRE(app.courses.view_roster(faculty02, "CS201"));
        const auto faculty01 = app.login("faculty01", "Teach1234");
        REQUIRE(app.courses.view_roster(faculty01, "CS201").error() ==
                ErrorCode::PERMISSION_DENIED);

        // Inactive course still rejects new enrollments after the restart.
        const auto rejected =
            app.courses.enroll_student(admin, "SRN-002", "CS202", 3);
        REQUIRE_FALSE(rejected);
        REQUIRE(rejected.error() == ErrorCode::INVALID_INPUT);

        // A removed student can be enrolled again.
        REQUIRE(app.courses.enroll_student(admin, "SRN-002", "CS201", 3));
    }

    {
        App app(test_database);
        REQUIRE(app.courses.is_enrolled("SRN-002", "CS201", 3));
    }

    std::remove(test_database);
}

TEST_CASE("Rejected operations leave nothing behind after a restart",
          "[course][enrolment][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        const auto admin = app.login("admin01", "Admin1234");
        const auto faculty = app.login("faculty01", "Teach1234");
        register_students(app, admin);

        REQUIRE(app.courses.create_course(
            admin, Course{"CS201", "Data Structures", 4, "faculty01"}));

        // Invalid references, invalid input and missing permission.
        REQUIRE_FALSE(app.courses.create_course(
            admin, Course{"CS300", "Ghost Course", 4, "nobody"}));
        REQUIRE_FALSE(app.courses.create_course(
            faculty, Course{"CS301", "Not Allowed", 4, "faculty01"}));
        REQUIRE_FALSE(app.courses.update_course(
            admin, Course{"CS999", "Unknown", 4, "faculty01"}));
        REQUIRE_FALSE(app.courses.enroll_student(
            admin, "SRN-999", "CS201", 3));
        REQUIRE_FALSE(app.courses.enroll_student(
            admin, "SRN-001", "NOPE", 3));
        REQUIRE_FALSE(app.courses.enroll_student(
            faculty, "SRN-001", "CS201", 3));
        REQUIRE_FALSE(app.courses.enroll_student(
            admin, "SRN-001", "CS201", 13));
    }

    {
        App app(test_database);
        const auto admin = app.login("admin01", "Admin1234");

        const auto courses = app.courses.list_courses(admin);
        REQUIRE(courses);
        REQUIRE(courses.value()->size() == 1);
        REQUIRE(courses.value()->front().code == "CS201");

        REQUIRE(app.courses.view_roster(admin, "CS201").value()->empty());
        REQUIRE_FALSE(app.courses.get_course("CS300"));
        REQUIRE_FALSE(app.courses.get_course("CS301"));
        REQUIRE_FALSE(app.courses.get_course("CS999"));
    }

    std::remove(test_database);
}
