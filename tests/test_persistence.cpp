#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>

#include "persistence/database.h"
#include "persistence/repositories.h"

namespace {
const char* test_database = "srs-persistence-test.db";
}

TEST_CASE("SQLite storage initializes, persists and reloads records", "[persistence]") {
    std::remove(test_database);
    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());
        srs::persistence::UserRepository users(database);
        srs::persistence::StudentRepository students(database);
        srs::persistence::CourseRepository courses(database);
        srs::persistence::EnrollmentRepository enrollments(database);
        REQUIRE(users.save({"student01", "hash", "Student", true}));
        REQUIRE(students.save({"SRN-001", "student01", "Alice", "2005-01-01",
                               "555", "CS", 3, true}));
        REQUIRE(courses.save({"CS101", "Programming", 4, "student01"}));
        REQUIRE(enrollments.save({"SRN-001", "CS101", 3}));
    }
    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());
        srs::persistence::StudentRepository students(database);
        const auto row = students.find("SRN-001");
        REQUIRE(row);
        REQUIRE(row.value()->name == "Alice");
    }
    std::remove(test_database);
}

TEST_CASE("SQLite constraints reject invalid relationships and duplicates", "[persistence]") {
    std::remove(test_database);
    srs::persistence::Database database(test_database);
    REQUIRE(database.is_open());
    srs::persistence::EnrollmentRepository enrollments(database);
    REQUIRE_FALSE(enrollments.save({"missing", "missing", 1}));
    srs::persistence::UserRepository users(database);
    REQUIRE(users.save({"admin", "hash", "Admin", true}));
    REQUIRE(users.save({"admin", "hash2", "Admin", true}));
    std::remove(test_database);
}

TEST_CASE("Database reports an error when the path cannot be opened", "[persistence]") {
    srs::persistence::Database database("/nonexistent-dir-srs/none/x.db");
    REQUIRE_FALSE(database.is_open());
    REQUIRE_FALSE(database.last_error().empty());
}

TEST_CASE("Transaction cannot be committed twice", "[persistence]") {
    std::remove(test_database);
    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());
        srs::persistence::Transaction transaction(database);
        REQUIRE(transaction.started());
        REQUIRE(transaction.commit().has_value());
        REQUIRE_FALSE(transaction.commit().has_value());
    }
    std::remove(test_database);
}

TEST_CASE("Transaction rolls back when it is not committed", "[persistence]") {
    std::remove(test_database);
    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());
        {
            srs::persistence::Transaction transaction(database);
            REQUIRE(transaction.started());
            REQUIRE(database.execute(
                "CREATE TABLE rollback_probe(x INTEGER);").has_value());
        }
        REQUIRE_FALSE(database.execute(
            "INSERT INTO rollback_probe VALUES (1);").has_value());
    }
    std::remove(test_database);
}

TEST_CASE("A second transaction cannot start while one is active", "[persistence]") {
    std::remove(test_database);
    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());
        srs::persistence::Transaction first(database);
        srs::persistence::Transaction second(database);
        REQUIRE(first.started());
        REQUIRE_FALSE(second.started());
        REQUIRE_FALSE(second.commit().has_value());
    }
    std::remove(test_database);
}
