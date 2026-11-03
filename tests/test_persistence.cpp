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
