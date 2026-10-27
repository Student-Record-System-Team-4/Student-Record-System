
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/validator.h"

using srs::core::ErrorCode;
using srs::core::Validator;

TEST_CASE("Username validation", "[validator][username]") {
    SECTION("Rejects an empty username") {
        auto result = Validator::validate_username("");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Rejects whitespace-only usernames") {
        auto result = Validator::validate_username("   \t ");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Accepts a valid username") {
        auto result = Validator::validate_username("student123");
        REQUIRE(result);
    }

    SECTION("Rejects usernames with surrounding whitespace") {
        auto result = Validator::validate_username(" student123 ");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Rejects embedded null characters") {
        const std::string username("student\0name", 12);
        auto result = Validator::validate_username(username);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Rejects usernames with unsupported characters") {
        auto result = Validator::validate_username("student@example");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Rejects usernames longer than the maximum length") {
        const std::string username(65, 'a');
        auto result = Validator::validate_username(username);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }
}

TEST_CASE("Password validation", "[validator][password]") {
    SECTION("Rejects passwords shorter than eight characters") {
        auto result = Validator::validate_password("Pass123");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Rejects an empty password") {
        auto result = Validator::validate_password("");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Accepts an eight-character password") {
        auto result = Validator::validate_password("Pass1234");
        REQUIRE(result);
    }

    SECTION("Accepts passwords longer than the minimum length") {
        auto result = Validator::validate_password("Pass12345");
        REQUIRE(result);
    }

    SECTION("Rejects whitespace-only passwords") {
        auto result = Validator::validate_password("        ");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Rejects embedded null characters") {
        const std::string password("Pass\0word123", 12);
        auto result = Validator::validate_password(password);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Rejects passwords containing control characters") {
        const std::string password("Pass123\001", 8);
        auto result = Validator::validate_password(password);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Rejects passwords longer than the maximum length") {
        const std::string password(129, 'a');
        auto result = Validator::validate_password(password);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }
}

TEST_CASE("Credential validation", "[validator][credentials]") {
    SECTION("Accepts valid credentials") {
        auto result =
            Validator::validate_credentials("student123", "Pass1234");
        REQUIRE(result);
    }

    SECTION("Rejects an invalid username") {
        auto result =
            Validator::validate_credentials("", "Pass1234");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }

    SECTION("Rejects an invalid password") {
        auto result =
            Validator::validate_credentials("student123", "short");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == ErrorCode::INVALID_INPUT);
    }
}
