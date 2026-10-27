#include <catch2/catch_test_macros.hpp>

#include "auth/authentication.h"
#include "student/student_profiles.h"

using srs::auth::AuthenticationService;
using srs::student::StudentProfile;
using srs::student::StudentProfileService;
using srs::student::StudentSearchQuery;
using srs::student::UserRole;

namespace {

StudentProfile profile(
    const std::string& username,
    const std::string& name,
    const std::string& srn,
    const std::string& programme,
    int semester) {
    return {username, name, srn, "2000-01-01", "555-0100",
            programme, semester, true};
}

}  // namespace

TEST_CASE("Admin can register and retrieve a student profile", "[student]") {
    AuthenticationService authentication(
        std::vector<srs::auth::User>{{"student123", "Pass1234"}});
    StudentProfileService students(authentication);

    REQUIRE(students.register_student(
                profile("student123", "Alice Smith", "SRN-001",
                        "Computer Science", 3),
                UserRole::Admin));

    const auto result =
        students.view_own_profile("student123", UserRole::Student);
    REQUIRE(result);
    REQUIRE(result.value()->srn == "SRN-001");
    REQUIRE(result.value()->programme == "Computer Science");
}

TEST_CASE("Student profile ownership is enforced", "[student]") {
    AuthenticationService authentication(
        std::vector<srs::auth::User>{
            {"student123", "Pass1234"}, {"student456", "Pass5678"}});
    StudentProfileService students(authentication);
    REQUIRE(students.register_student(
        profile("student123", "Alice Smith", "SRN-001",
                "Computer Science", 3),
        UserRole::Admin));

    REQUIRE_FALSE(students.view_own_profile(
        "student456", UserRole::Student));
    REQUIRE_FALSE(students.view_own_profile(
        "student123", UserRole::Admin));
}

TEST_CASE("Admin can update and deactivate a student", "[student]") {
    AuthenticationService authentication(
        std::vector<srs::auth::User>{{"student123", "Pass1234"}});
    StudentProfileService students(authentication);
    REQUIRE(students.register_student(
        profile("student123", "Alice Smith", "SRN-001",
                "Computer Science", 3),
        UserRole::Admin));

    StudentProfile updated = profile(
        "student123", "Alice Jones", "SRN-001", "Data Science", 4);
    REQUIRE(students.update_student(
        "SRN-001", updated, UserRole::Admin));
    REQUIRE(students.view_own_profile(
                "student123", UserRole::Student)
                .value()
                ->name == "Alice Jones");

    REQUIRE(students.deactivate_student("SRN-001", UserRole::Admin));
    REQUIRE(authentication.login("student123", "Pass1234") == nullptr);
    REQUIRE_FALSE(students.view_own_profile(
        "student123", UserRole::Student));
}

TEST_CASE("Admin can search students by profile fields", "[student]") {
    AuthenticationService authentication(
        std::vector<srs::auth::User>{
            {"student123", "Pass1234"}, {"student456", "Pass5678"}});
    StudentProfileService students(authentication);
    REQUIRE(students.register_student(
        profile("student123", "Alice Smith", "SRN-001",
                "Computer Science", 3),
        UserRole::Admin));
    REQUIRE(students.register_student(
        profile("student456", "Bob Jones", "SRN-002",
                "Business", 2),
        UserRole::Admin));

    const auto result = students.search_students(
        StudentSearchQuery{"alice", "", "", 0}, UserRole::Admin);
    REQUIRE(result);
    REQUIRE(result.value()->size() == 1);
    REQUIRE(result.value()->front().srn == "SRN-001");

    const auto semester_result = students.search_students(
        StudentSearchQuery{"", "", "", 2}, UserRole::Admin);
    REQUIRE(semester_result);
    REQUIRE(semester_result.value()->size() == 1);
    REQUIRE(semester_result.value()->front().username == "student456");
}

TEST_CASE("Invalid and duplicate student records are rejected", "[student]") {
    AuthenticationService authentication(
        std::vector<srs::auth::User>{{"student123", "Pass1234"}});
    StudentProfileService students(authentication);
    const StudentProfile valid = profile(
        "student123", "Alice Smith", "SRN-001", "Computer Science", 3);

    REQUIRE_FALSE(students.register_student(valid, UserRole::Student));
    REQUIRE(students.register_student(valid, UserRole::Admin));
    REQUIRE_FALSE(students.register_student(valid, UserRole::Admin));

    const StudentProfile invalid = profile(
        "student123", "Alice Smith", "bad srn", "Computer Science", 3);
    REQUIRE_FALSE(students.update_student(
        "SRN-001", invalid, UserRole::Admin));
}
