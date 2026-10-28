#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "auth/access_control.h"
#include "auth/authentication.h"
#include "auth/user_accounts.h"

using srs::auth::AuthenticationService;
using srs::auth::Permission;
using srs::auth::Role;
using srs::auth::User;
using srs::auth::UserAccountService;
using srs::core::ErrorCode;

namespace {

std::vector<User> seed_users() {
    return {{"admin01", "Admin1234", Role::Admin},
            {"faculty01", "Teach1234", Role::Faculty},
            {"student01", "Pass1234", Role::Student}};
}

std::string login_id(AuthenticationService& auth,
                     const std::string& username,
                     const std::string& password) {
    const auto session = auth.login(username, password);
    REQUIRE(session != nullptr);
    return session->id;
}

}  // namespace

TEST_CASE("Role is assigned and enforced per session", "[rbac][accounts]") {
    AuthenticationService auth(seed_users());
    UserAccountService accounts(auth);

    const auto admin = login_id(auth, "admin01", "Admin1234");
    const auto faculty = login_id(auth, "faculty01", "Teach1234");
    const auto student = login_id(auth, "student01", "Pass1234");

    REQUIRE(accounts.authorize(admin, Permission::ManageUsers));
    REQUIRE(accounts.authorize(faculty, Permission::EnterGrades));
    REQUIRE(accounts.authorize(student, Permission::ViewOwnGrades));

    auto denied = accounts.authorize(student, Permission::EnterGrades);
    REQUIRE_FALSE(denied);
    REQUIRE(denied.error() == ErrorCode::PERMISSION_DENIED);

    denied = accounts.authorize(faculty, Permission::ManageUsers);
    REQUIRE_FALSE(denied);
    REQUIRE(denied.error() == ErrorCode::PERMISSION_DENIED);
}

TEST_CASE("Unauthenticated or logged-out sessions are refused", "[rbac][accounts]") {
    AuthenticationService auth(seed_users());
    UserAccountService accounts(auth);

    auto result = accounts.authorize("forged-session", Permission::ViewOwnProfile);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == ErrorCode::SESSION_ERROR);

    const auto admin = login_id(auth, "admin01", "Admin1234");
    REQUIRE(auth.logout(admin));
    result = accounts.create_user(admin, {"newuser1", "Pass1234", Role::Student});
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == ErrorCode::SESSION_ERROR);
    REQUIRE_FALSE(auth.is_registered("newuser1"));
}

TEST_CASE("Only admins can manage accounts", "[accounts]") {
    AuthenticationService auth(seed_users());
    UserAccountService accounts(auth);
    const auto student = login_id(auth, "student01", "Pass1234");
    const auto faculty = login_id(auth, "faculty01", "Teach1234");

    const User intruder{"intruder1", "Pass1234", Role::Admin};
    auto result = accounts.create_user(student, intruder);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == ErrorCode::PERMISSION_DENIED);
    REQUIRE_FALSE(auth.is_registered("intruder1"));

    REQUIRE_FALSE(accounts.set_user_role(faculty, "student01", Role::Admin));
    REQUIRE_FALSE(accounts.set_user_active(faculty, "student01", false));
    REQUIRE_FALSE(accounts.reset_password(student, "admin01", "Hacked1234"));
    REQUIRE(auth.login("admin01", "Admin1234") != nullptr);
}

TEST_CASE("Admin can create accounts", "[accounts]") {
    AuthenticationService auth(seed_users());
    UserAccountService accounts(auth);
    const auto admin = login_id(auth, "admin01", "Admin1234");

    REQUIRE(accounts.create_user(admin, {"faculty02", "Teach5678", Role::Faculty}));
    const auto session = auth.login("faculty02", "Teach5678");
    REQUIRE(session != nullptr);
    REQUIRE(accounts.authorize(session->id, Permission::MarkAttendance));

    auto duplicate = accounts.create_user(admin, {"faculty02", "Other12345", Role::Student});
    REQUIRE_FALSE(duplicate);
    REQUIRE(duplicate.error() == ErrorCode::ALREADY_EXISTS);

    auto bad_name = accounts.create_user(admin, {"bad name!", "Pass1234", Role::Student});
    REQUIRE_FALSE(bad_name);
    REQUIRE(bad_name.error() == ErrorCode::INVALID_INPUT);

    auto bad_password = accounts.create_user(admin, {"goodname", "short", Role::Student});
    REQUIRE_FALSE(bad_password);
    REQUIRE(bad_password.error() == ErrorCode::INVALID_INPUT);
    REQUIRE_FALSE(auth.is_registered("goodname"));
}

TEST_CASE("Deactivation is enforced and reversible", "[accounts]") {
    AuthenticationService auth(seed_users());
    UserAccountService accounts(auth);
    const auto admin = login_id(auth, "admin01", "Admin1234");
    const auto student = login_id(auth, "student01", "Pass1234");

    REQUIRE(accounts.set_user_active(admin, "student01", false));
    REQUIRE_FALSE(auth.is_authenticated(student));
    REQUIRE(auth.login("student01", "Pass1234") == nullptr);

    REQUIRE(accounts.set_user_active(admin, "student01", true));
    REQUIRE(auth.login("student01", "Pass1234") != nullptr);

    auto missing = accounts.set_user_active(admin, "nobody", false);
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error() == ErrorCode::NOT_FOUND);
}

TEST_CASE("Role changes take effect immediately", "[accounts][rbac]") {
    AuthenticationService auth(seed_users());
    UserAccountService accounts(auth);
    const auto admin = login_id(auth, "admin01", "Admin1234");
    const auto faculty = login_id(auth, "faculty01", "Teach1234");

    REQUIRE(accounts.authorize(faculty, Permission::EnterGrades));
    REQUIRE(accounts.set_user_role(admin, "faculty01", Role::Student));
    REQUIRE_FALSE(accounts.authorize(faculty, Permission::EnterGrades));
    REQUIRE(accounts.authorize(faculty, Permission::ViewOwnGrades));

    auto missing = accounts.set_user_role(admin, "nobody", Role::Admin);
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error() == ErrorCode::NOT_FOUND);
}

TEST_CASE("An admin cannot lock themselves out", "[accounts]") {
    AuthenticationService auth(seed_users());
    UserAccountService accounts(auth);
    const auto admin = login_id(auth, "admin01", "Admin1234");

    REQUIRE_FALSE(accounts.set_user_active(admin, "admin01", false));
    REQUIRE_FALSE(accounts.set_user_role(admin, "admin01", Role::Student));
    REQUIRE(auth.is_authenticated(admin));
    REQUIRE(accounts.authorize(admin, Permission::ManageUsers));
}

TEST_CASE("Admin can reset a password", "[accounts][password]") {
    AuthenticationService auth(seed_users());
    UserAccountService accounts(auth);
    const auto admin = login_id(auth, "admin01", "Admin1234");
    const auto student = login_id(auth, "student01", "Pass1234");

    REQUIRE(accounts.reset_password(admin, "student01", "Fresh12345"));
    REQUIRE_FALSE(auth.is_authenticated(student));
    REQUIRE(auth.login("student01", "Pass1234") == nullptr);
    REQUIRE(auth.login("student01", "Fresh12345") != nullptr);

    REQUIRE_FALSE(accounts.reset_password(admin, "student01", "short"));
    auto missing = accounts.reset_password(admin, "nobody", "Fresh12345");
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error() == ErrorCode::NOT_FOUND);
}

TEST_CASE("Users can change their own password", "[accounts][password]") {
    AuthenticationService auth(seed_users());
    UserAccountService accounts(auth);
    const auto student = login_id(auth, "student01", "Pass1234");

    auto wrong = accounts.change_password(student, "NotMyPass1", "NewPass5678");
    REQUIRE_FALSE(wrong);
    REQUIRE(wrong.error() == ErrorCode::INVALID_CREDENTIALS);

    auto same = accounts.change_password(student, "Pass1234", "Pass1234");
    REQUIRE_FALSE(same);
    REQUIRE(same.error() == ErrorCode::INVALID_INPUT);

    auto weak = accounts.change_password(student, "Pass1234", "short");
    REQUIRE_FALSE(weak);
    REQUIRE(weak.error() == ErrorCode::INVALID_INPUT);

    REQUIRE(accounts.change_password(student, "Pass1234", "NewPass5678"));
    REQUIRE(auth.is_authenticated(student));
    REQUIRE(auth.login("student01", "Pass1234") == nullptr);
    REQUIRE(auth.login("student01", "NewPass5678") != nullptr);

    auto no_session = accounts.change_password("forged", "NewPass5678", "Another1234");
    REQUIRE_FALSE(no_session);
    REQUIRE(no_session.error() == ErrorCode::SESSION_ERROR);
}
