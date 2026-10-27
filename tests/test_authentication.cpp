#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "auth/authentication.h"

using srs::auth::AuthenticationService;
using srs::auth::User;

TEST_CASE("A registered user can log in", "[auth][login]") {
    AuthenticationService authentication(
        std::vector<User>{{"student123", "Pass1234"}});

    const auto session = authentication.login("student123", "Pass1234");

    REQUIRE(session != nullptr);
    REQUIRE(session->username == "student123");
    REQUIRE(authentication.is_authenticated(session->id));
}

TEST_CASE("Invalid credentials are rejected", "[auth][login]") {
    AuthenticationService authentication(
        std::vector<User>{{"student123", "Pass1234"}});

    REQUIRE(authentication.login("student123", "wrong") == nullptr);
    REQUIRE(authentication.login("unknown", "Pass1234") == nullptr);
}

TEST_CASE("Logging out terminates the current session", "[auth][logout]") {
    AuthenticationService authentication(
        std::vector<User>{{"student123", "Pass1234"}});
    const auto session = authentication.login("student123", "Pass1234");
    REQUIRE(session != nullptr);

    REQUIRE(authentication.logout(session->id));
    REQUIRE_FALSE(authentication.is_authenticated(session->id));
    REQUIRE_FALSE(authentication.logout(session->id));
}

TEST_CASE("Sessions are independent", "[auth][session]") {
    AuthenticationService authentication(std::vector<User>{
        {"student123", "Pass1234"},
        {"faculty123", "Teach1234"}});

    const auto student_session =
        authentication.login("student123", "Pass1234");
    const auto faculty_session =
        authentication.login("faculty123", "Teach1234");
    REQUIRE(student_session != nullptr);
    REQUIRE(faculty_session != nullptr);
    REQUIRE(student_session->id != faculty_session->id);

    REQUIRE(authentication.logout(student_session->id));
    REQUIRE_FALSE(authentication.is_authenticated(student_session->id));
    REQUIRE(authentication.is_authenticated(faculty_session->id));
}
