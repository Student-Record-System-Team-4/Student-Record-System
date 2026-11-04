#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>
#include <vector>

#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "course/course_service.h"
#include "grades/grade_service.h"
#include "persistence/database.h"
#include "student/student_profiles.h"

// SRS-F-013..F-016: grade data must survive a restart. These tests drive
// GradeService itself and "restart" by destroying every service and reopening
// the same database file.

using srs::auth::AuthenticationService;
using srs::auth::Role;
using srs::auth::User;
using srs::auth::UserAccountService;
using srs::core::ErrorCode;
using srs::course::Course;
using srs::course::CourseService;
using srs::grades::GradeService;
using srs::persistence::Database;
using srs::student::StudentProfile;
using srs::student::StudentProfileService;

namespace {

const char* test_database = "srs-grade-service-persistence-test.db";

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
    GradeService grades;

    explicit App(const char* path)
        : database(path),
          auth(seed_users(), database),
          accounts(auth),
          students(auth, database),
          courses(accounts, auth, students, database),
          grades(accounts, auth, students, courses, database) {
        REQUIRE(database.is_open());
    }

    std::string login(const std::string& user, const std::string& password) {
        const auto session = auth.login(user, password);
        REQUIRE(session != nullptr);
        return session->id;
    }
};

// Alice (SRN-001): CS201 (4cr) and CS202 (3cr) in semester 3, CS203 (2cr) in
// semester 4. Bob (SRN-002): CS201 in semester 3.
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
    REQUIRE(app.courses.create_course(
        admin, Course{"CS202", "Databases", 3, "faculty02"}));
    REQUIRE(app.courses.create_course(
        admin, Course{"CS203", "Networks", 2, "faculty01"}));

    REQUIRE(app.courses.enroll_student(admin, "SRN-001", "CS201", 3));
    REQUIRE(app.courses.enroll_student(admin, "SRN-001", "CS202", 3));
    REQUIRE(app.courses.enroll_student(admin, "SRN-001", "CS203", 4));
    REQUIRE(app.courses.enroll_student(admin, "SRN-002", "CS201", 3));
}

}  // namespace

TEST_CASE("Grades and GPA survive a restart", "[grades][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        seed_academic_data(app);

        const auto faculty1 = app.login("faculty01", "Teach1234");
        const auto faculty2 = app.login("faculty02", "Teach5678");

        REQUIRE(app.grades.enter_marks(faculty1, "SRN-001", "CS201", 3, 95));  // S=10
        REQUIRE(app.grades.enter_letter_grade(
            faculty2, "SRN-001", "CS202", 3, "b"));                           // 8
        REQUIRE(app.grades.enter_marks(faculty1, "SRN-001", "CS203", 4, 65));  // C=7
        REQUIRE(app.grades.enter_marks(faculty1, "SRN-002", "CS201", 3, 42));  // E=5
    }

    {
        App app(test_database);
        const auto alice = app.login("student01", "Pass1234");
        const auto bob = app.login("student02", "Pass5678");
        const auto admin = app.login("admin01", "Admin1234");

        const auto grades = app.grades.view_my_grades(alice);
        REQUIRE(grades);
        REQUIRE(grades.value()->size() == 3);

        // Map order: by course code, then semester.
        const auto& cs201 = grades.value()->at(0);
        REQUIRE(cs201.course_code == "CS201");
        REQUIRE(cs201.credits == 4);
        REQUIRE(cs201.has_marks);
        REQUIRE(cs201.marks == 95.0);
        REQUIRE(cs201.letter == "S");
        REQUIRE(cs201.grade_points == 10.0);

        const auto& cs202 = grades.value()->at(1);
        REQUIRE(cs202.course_code == "CS202");
        REQUIRE_FALSE(cs202.has_marks);
        REQUIRE(cs202.letter == "B");
        REQUIRE(cs202.grade_points == 8.0);

        // GPA/CGPA are recomputed from the reloaded grades.
        const auto summary = app.grades.view_my_summary(alice);
        REQUIRE(summary);
        REQUIRE(summary.value()->semesters.size() == 2);
        REQUIRE(summary.value()->semesters[0].gpa == 9.14);
        REQUIRE(summary.value()->semesters[1].gpa == 7.0);
        REQUIRE(summary.value()->total_credits == 9);
        REQUIRE(summary.value()->cgpa == 8.67);

        // Students still only see their own records.
        const auto bobs = app.grades.view_my_grades(bob);
        REQUIRE(bobs);
        REQUIRE(bobs.value()->size() == 1);
        REQUIRE(bobs.value()->front().srn == "SRN-002");
        REQUIRE(bobs.value()->front().letter == "E");

        const auto as_admin = app.grades.view_student_summary(admin, "SRN-001");
        REQUIRE(as_admin);
        REQUIRE(as_admin.value()->cgpa == 8.67);
    }

    std::remove(test_database);
}

TEST_CASE("Grade updates persist and replace the earlier grade",
          "[grades][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        seed_academic_data(app);
        const auto faculty1 = app.login("faculty01", "Teach1234");

        REQUIRE(app.grades.enter_marks(faculty1, "SRN-001", "CS201", 3, 55));  // D=6
    }

    {
        App app(test_database);
        const auto faculty1 = app.login("faculty01", "Teach1234");
        const auto alice = app.login("student01", "Pass1234");

        REQUIRE(app.grades.view_my_grades(alice).value()->front().letter == "D");

        // Modify after the restart, switching from marks to a letter grade.
        REQUIRE(app.grades.enter_letter_grade(faculty1, "SRN-001", "CS201", 3, "A"));
    }

    {
        App app(test_database);
        const auto alice = app.login("student01", "Pass1234");

        const auto grades = app.grades.view_my_grades(alice);
        REQUIRE(grades);
        REQUIRE(grades.value()->size() == 1);  // updated, not duplicated
        REQUIRE_FALSE(grades.value()->front().has_marks);
        REQUIRE(grades.value()->front().letter == "A");
        REQUIRE(grades.value()->front().grade_points == 9.0);
        REQUIRE(app.grades.view_my_summary(alice).value()->cgpa == 9.0);
    }

    std::remove(test_database);
}

TEST_CASE("GPA follows persisted course credit changes after a restart",
          "[grades][gpa][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        seed_academic_data(app);
        const auto faculty1 = app.login("faculty01", "Teach1234");
        const auto faculty2 = app.login("faculty02", "Teach5678");

        REQUIRE(app.grades.enter_marks(faculty1, "SRN-001", "CS201", 3, 95));  // 10, 4cr
        REQUIRE(app.grades.enter_letter_grade(
            faculty2, "SRN-001", "CS202", 3, "B"));                           // 8, 3cr
    }

    {
        App app(test_database);
        const auto admin = app.login("admin01", "Admin1234");
        const auto alice = app.login("student01", "Pass1234");

        REQUIRE(app.grades.view_my_summary(alice).value()->cgpa == 9.14);

        // CS201 drops to 1 credit: (10*1 + 8*3) / 4 = 8.5.
        REQUIRE(app.courses.update_course(
            admin, Course{"CS201", "Data Structures", 1, "faculty01"}));
        REQUIRE(app.grades.view_my_summary(alice).value()->cgpa == 8.5);
    }

    {
        App app(test_database);
        const auto alice = app.login("student01", "Pass1234");

        const auto summary = app.grades.view_my_summary(alice);
        REQUIRE(summary.value()->total_credits == 4);
        REQUIRE(summary.value()->cgpa == 8.5);
    }

    std::remove(test_database);
}

TEST_CASE("Rejected grade entries leave nothing behind after a restart",
          "[grades][persistence]") {
    std::remove(test_database);

    {
        App app(test_database);
        seed_academic_data(app);

        const auto faculty1 = app.login("faculty01", "Teach1234");
        const auto faculty2 = app.login("faculty02", "Teach5678");
        const auto alice = app.login("student01", "Pass1234");

        // Invalid values.
        REQUIRE_FALSE(app.grades.enter_marks(faculty1, "SRN-001", "CS201", 3, -1));
        REQUIRE_FALSE(app.grades.enter_marks(faculty1, "SRN-001", "CS201", 3, 100.5));
        REQUIRE_FALSE(app.grades.enter_letter_grade(
            faculty1, "SRN-001", "CS201", 3, "Z"));

        // Student not enrolled in that course / semester.
        REQUIRE_FALSE(app.grades.enter_marks(faculty1, "SRN-002", "CS203", 4, 80));
        REQUIRE_FALSE(app.grades.enter_marks(faculty1, "SRN-001", "CS201", 4, 80));

        // Wrong faculty, and roles that may not grade at all.
        REQUIRE_FALSE(app.grades.enter_marks(faculty2, "SRN-001", "CS201", 3, 80));
        REQUIRE_FALSE(app.grades.enter_marks(alice, "SRN-001", "CS201", 3, 80));
    }

    {
        App app(test_database);
        const auto alice = app.login("student01", "Pass1234");
        const auto bob = app.login("student02", "Pass5678");

        REQUIRE(app.grades.view_my_grades(alice).value()->empty());
        REQUIRE(app.grades.view_my_grades(bob).value()->empty());
        REQUIRE(app.grades.view_my_summary(alice).value()->cgpa == 0.0);
    }

    std::remove(test_database);
}
