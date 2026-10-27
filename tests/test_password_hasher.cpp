
#include <catch2/catch_test_macros.hpp>
#include <string>

#include "auth/password_hasher.h"

TEST_CASE("Password hashing and verification", "[auth][password]") {
    const std::string password = "SecurePass123";

    auto hashed = srs::auth::PasswordHasher::hash_password(password);

    REQUIRE(hashed);
    REQUIRE(hashed.value() != nullptr);

    const std::string stored_hash = *hashed.value();

    SECTION("Stored hash is not the plaintext password") {
        REQUIRE(stored_hash != password);
    }

    SECTION("Correct password is accepted") {
        auto result = srs::auth::PasswordHasher::verify_password(
            password, stored_hash);

        REQUIRE(result);
        REQUIRE(result.value() != nullptr);
        REQUIRE(*result.value());
    }

    SECTION("Incorrect password is rejected") {
        auto result = srs::auth::PasswordHasher::verify_password(
            "WrongPass123", stored_hash);

        REQUIRE(result);
        REQUIRE(result.value() != nullptr);
        REQUIRE_FALSE(*result.value());
    }

    SECTION("Same password receives a different salted hash") {
        auto second_hash =
            srs::auth::PasswordHasher::hash_password(password);

        REQUIRE(second_hash);
        REQUIRE(second_hash.value() != nullptr);
        REQUIRE(*second_hash.value() != stored_hash);
    }
}
