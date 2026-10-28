#include <catch2/catch_test_macros.hpp>

#include "auth/access_control.h"

using srs::auth::AccessControl;
using srs::auth::Permission;
using srs::auth::Role;

TEST_CASE("Students can only reach their own records", "[rbac]") {
    REQUIRE(AccessControl::is_allowed(Role::Student, Permission::ViewOwnProfile));
    REQUIRE(AccessControl::is_allowed(Role::Student, Permission::ViewOwnGrades));
    REQUIRE(AccessControl::is_allowed(Role::Student, Permission::ViewOwnAttendance));
    REQUIRE(AccessControl::is_allowed(Role::Student, Permission::ViewOwnReports));

    REQUIRE_FALSE(AccessControl::is_allowed(Role::Student, Permission::EnterGrades));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Student, Permission::MarkAttendance));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Student, Permission::ViewCourseRoster));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Student, Permission::SearchStudents));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Student, Permission::ManageUsers));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Student, Permission::GenerateInstitutionReports));
}

TEST_CASE("Faculty can grade and mark attendance but not administer", "[rbac]") {
    REQUIRE(AccessControl::is_allowed(Role::Faculty, Permission::EnterGrades));
    REQUIRE(AccessControl::is_allowed(Role::Faculty, Permission::MarkAttendance));
    REQUIRE(AccessControl::is_allowed(Role::Faculty, Permission::ViewCourseRoster));
    REQUIRE(AccessControl::is_allowed(Role::Faculty, Permission::ViewCourseSummary));

    REQUIRE_FALSE(AccessControl::is_allowed(Role::Faculty, Permission::ViewOwnGrades));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Faculty, Permission::ManageUsers));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Faculty, Permission::ManageStudents));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Faculty, Permission::ManageCourses));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Faculty, Permission::GenerateInstitutionReports));
}

TEST_CASE("Admins administer the system but do not enter grades", "[rbac]") {
    REQUIRE(AccessControl::is_allowed(Role::Admin, Permission::ManageUsers));
    REQUIRE(AccessControl::is_allowed(Role::Admin, Permission::ManageStudents));
    REQUIRE(AccessControl::is_allowed(Role::Admin, Permission::SearchStudents));
    REQUIRE(AccessControl::is_allowed(Role::Admin, Permission::ManageCourses));
    REQUIRE(AccessControl::is_allowed(Role::Admin, Permission::EnrollStudents));
    REQUIRE(AccessControl::is_allowed(Role::Admin, Permission::ViewCourseRoster));
    REQUIRE(AccessControl::is_allowed(Role::Admin, Permission::GenerateInstitutionReports));

    REQUIRE_FALSE(AccessControl::is_allowed(Role::Admin, Permission::EnterGrades));
    REQUIRE_FALSE(AccessControl::is_allowed(Role::Admin, Permission::MarkAttendance));
}
