#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

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
using srs::student::StudentProfile;
using srs::student::StudentProfileService;
using srs::student::UserRole;

namespace {

// One admin, two faculty, two students; profiles registered for both students.
struct Fixture {
    AuthenticationService auth{std::vector<User>{
        {"admin01", "Admin1234", Role::Admin},
        {"faculty01", "Teach1234", Role::Faculty},
        {"faculty02", "Teach5678", Role::Faculty},
        {"student01", "Pass1234", Role::Student},
        {"student02", "Pass5678", Role::Student}}};
    UserAccountService accounts{auth};
    StudentProfileService students{auth};
    CourseService courses{accounts, auth, students};

    std::string admin;
    std::string faculty1;
    std::string faculty2;
    std::string student;

    Fixture() {
        REQUIRE(students.register_student(
            StudentProfile{"student01", "Alice Smith", "SRN-001", "2005-01-01",
                           "555-0100", "Computer Science", 3, true},
            UserRole::Admin));
        REQUIRE(students.register_student(
            StudentProfile{"student02", "Bob Jones", "SRN-002", "2005-02-02",
                           "555-0101", "Computer Science", 3, true},
            UserRole::Admin));
        admin = login("admin01", "Admin1234");
        faculty1 = login("faculty01", "Teach1234");
        faculty2 = login("faculty02", "Teach5678");
        student = login("student01", "Pass1234");
    }

    std::string login(const std::string& username, const std::string& password) {
        const auto session = auth.login(username, password);
        REQUIRE(session != nullptr);
        return session->id;
    }
};

Course course(const std::string& code, const std::string& faculty = "faculty01") {
    return Course{code, "Data Structures", 4, faculty};
}

}  // namespace

TEST_CASE("Admin can create a course and see it in the list", "[course]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201")));

    const auto list = f.courses.list_courses(f.admin);
    REQUIRE(list);
    REQUIRE(list.value()->size() == 1);
    const Course& stored = list.value()->front();
    REQUIRE(stored.code == "CS201");
    REQUIRE(stored.title == "Data Structures");
    REQUIRE(stored.credits == 4);
    REQUIRE(stored.faculty_username == "faculty01");
}

TEST_CASE("Invalid and duplicate courses are rejected", "[course]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201")));

    auto duplicate = f.courses.create_course(f.admin, course("CS201"));
    REQUIRE_FALSE(duplicate);
    REQUIRE(duplicate.error() == ErrorCode::ALREADY_EXISTS);

    REQUIRE(f.courses.create_course(f.admin, course("X")).error() == ErrorCode::INVALID_INPUT);
    REQUIRE(f.courses.create_course(f.admin, course("CS 202")).error() == ErrorCode::INVALID_INPUT);
    REQUIRE(f.courses.create_course(f.admin, Course{"CS203", "", 4, "faculty01"}).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.courses.create_course(f.admin, Course{"CS203", "   ", 4, "faculty01"}).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.courses.create_course(f.admin, Course{"CS203", "Ok", 0, "faculty01"}).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.courses.create_course(f.admin, Course{"CS203", "Ok", 11, "faculty01"}).error() ==
            ErrorCode::INVALID_INPUT);
    // Assigned person must exist and hold the Faculty role.
    REQUIRE(f.courses.create_course(f.admin, course("CS204", "nobody")).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.courses.create_course(f.admin, course("CS205", "student01")).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.courses.create_course(f.admin, course("CS206", "admin01")).error() ==
            ErrorCode::INVALID_INPUT);

    REQUIRE(f.courses.list_courses(f.admin).value()->size() == 1);
}

TEST_CASE("Only admins can manage courses and enrolment", "[course][rbac]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201")));

    for (const auto& session : {f.faculty1, f.student}) {
        REQUIRE(f.courses.create_course(session, course("CS999")).error() ==
                ErrorCode::PERMISSION_DENIED);
        REQUIRE(f.courses.update_course(session, course("CS201")).error() ==
                ErrorCode::PERMISSION_DENIED);
        REQUIRE(f.courses.enroll_student(session, "SRN-001", "CS201", 3).error() ==
                ErrorCode::PERMISSION_DENIED);
    }
    REQUIRE(f.courses.create_course("forged", course("CS999")).error() ==
            ErrorCode::SESSION_ERROR);
    REQUIRE(f.courses.list_courses(f.admin).value()->size() == 1);
    REQUIRE_FALSE(f.courses.is_enrolled("SRN-001", "CS201"));
}

TEST_CASE("Admin can update a course", "[course]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201")));

    REQUIRE(f.courses.update_course(
        f.admin, Course{"CS201", "Advanced Data Structures", 3, "faculty02"}));
    const auto list = f.courses.list_courses(f.admin);
    REQUIRE(list.value()->front().title == "Advanced Data Structures");
    REQUIRE(list.value()->front().credits == 3);
    REQUIRE(f.courses.is_assigned_faculty("faculty02", "CS201"));
    REQUIRE_FALSE(f.courses.is_assigned_faculty("faculty01", "CS201"));

    REQUIRE(f.courses.update_course(f.admin, course("CS404")).error() == ErrorCode::NOT_FOUND);
    REQUIRE(f.courses.update_course(f.admin, Course{"CS201", "T", 99, "faculty01"}).error() ==
            ErrorCode::INVALID_INPUT);
}

TEST_CASE("Faculty see only their own courses; students see none", "[course][rbac]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201", "faculty01")));
    REQUIRE(f.courses.create_course(f.admin, course("CS202", "faculty02")));

    const auto mine = f.courses.list_courses(f.faculty1);
    REQUIRE(mine);
    REQUIRE(mine.value()->size() == 1);
    REQUIRE(mine.value()->front().code == "CS201");

    REQUIRE(f.courses.list_courses(f.admin).value()->size() == 2);
    REQUIRE(f.courses.list_courses(f.student).error() == ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.courses.list_courses("forged").error() == ErrorCode::SESSION_ERROR);
}

TEST_CASE("Enrolled students appear on the course roster", "[enrolment]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201")));
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-002", "CS201", 3));
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-001", "CS201", 3));

    const auto roster = f.courses.view_roster(f.admin, "CS201");
    REQUIRE(roster);
    REQUIRE(roster.value()->size() == 2);
    REQUIRE((*roster.value())[0].srn == "SRN-001");  // sorted by SRN
    REQUIRE((*roster.value())[0].name == "Alice Smith");
    REQUIRE((*roster.value())[1].srn == "SRN-002");
    REQUIRE(f.courses.is_enrolled("SRN-001", "CS201", 3));
    REQUIRE_FALSE(f.courses.is_enrolled("SRN-001", "CS201", 4));
    REQUIRE(f.courses.is_enrolled("SRN-001", "CS201"));
}

TEST_CASE("Enrolment rejects bad requests", "[enrolment]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201")));
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-001", "CS201", 3));

    REQUIRE(f.courses.enroll_student(f.admin, "SRN-001", "CS201", 3).error() ==
            ErrorCode::ALREADY_EXISTS);
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-001", "CS201", 4));  // another semester is fine
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-999", "CS201", 3).error() ==
            ErrorCode::NOT_FOUND);
    REQUIRE(f.courses.enroll_student(f.admin, "SRN", "CS201", 3).error() ==
            ErrorCode::NOT_FOUND);  // partial SRN must not match
    REQUIRE(f.courses.enroll_student(f.admin, "", "CS201", 3).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-002", "NOPE", 3).error() ==
            ErrorCode::NOT_FOUND);
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-002", "CS201", 0).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-002", "CS201", 13).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE_FALSE(f.courses.is_enrolled("SRN-002", "CS201"));
}

TEST_CASE("Deactivated students cannot be enrolled and leave the roster", "[enrolment]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201")));
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-001", "CS201", 3));
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-002", "CS201", 3));

    REQUIRE(f.students.deactivate_student("SRN-001", UserRole::Admin));

    REQUIRE(f.courses.enroll_student(f.admin, "SRN-001", "CS201", 4).error() ==
            ErrorCode::NOT_FOUND);
    const auto roster = f.courses.view_roster(f.admin, "CS201");
    REQUIRE(roster.value()->size() == 1);
    REQUIRE(roster.value()->front().srn == "SRN-002");
}

TEST_CASE("Roster access follows role and course assignment", "[roster][rbac]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201", "faculty01")));
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-001", "CS201", 3));

    REQUIRE(f.courses.view_roster(f.faculty1, "CS201"));  // assigned faculty

    auto other = f.courses.view_roster(f.faculty2, "CS201");  // different faculty
    REQUIRE_FALSE(other);
    REQUIRE(other.error() == ErrorCode::PERMISSION_DENIED);

    REQUIRE(f.courses.view_roster(f.student, "CS201").error() == ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.courses.view_roster("forged", "CS201").error() == ErrorCode::SESSION_ERROR);
    REQUIRE(f.courses.view_roster(f.admin, "NOPE").error() == ErrorCode::NOT_FOUND);
}

TEST_CASE("Roster can be filtered by semester", "[roster]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201")));
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-001", "CS201", 3));
    REQUIRE(f.courses.enroll_student(f.admin, "SRN-002", "CS201", 4));

    const auto sem3 = f.courses.view_roster(f.admin, "CS201", 3);
    REQUIRE(sem3.value()->size() == 1);
    REQUIRE(sem3.value()->front().srn == "SRN-001");
    REQUIRE(f.courses.view_roster(f.admin, "CS201", 5).value()->empty());
    REQUIRE(f.courses.view_roster(f.admin, "CS201").value()->size() == 2);
}

TEST_CASE("A course can be looked up by code", "[course]") {
    Fixture f;
    REQUIRE(f.courses.create_course(f.admin, course("CS201")));

    const auto found = f.courses.get_course("CS201");
    REQUIRE(found);
    REQUIRE(found.value()->credits == 4);
    REQUIRE(f.courses.get_course("NOPE").error() == ErrorCode::NOT_FOUND);
}
