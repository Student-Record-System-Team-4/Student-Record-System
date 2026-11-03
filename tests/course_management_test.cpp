#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>

#include "persistence/database.h"
#include "persistence/repositories.h"

namespace {

const char* test_database = "srs-course-management-test.db";

void clean_database() {
    std::remove(test_database);
}

void seed_required_records(srs::persistence::Database& database) {
    srs::persistence::UserRepository users(database);
    srs::persistence::StudentRepository students(database);

    REQUIRE(users.save({"faculty01", "hash", "Faculty", true}));
    REQUIRE(users.save({"student01", "hash", "Student", true}));

    REQUIRE(students.save({
        "SRN-001",
        "student01",
        "Alice Smith",
        "2005-01-01",
        "555-0100",
        "Computer Science",
        3,
        true
    }));
}

}  // namespace

TEST_CASE("Course can be created and retrieved", "[course-management]") {
    clean_database();

    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());

        seed_required_records(database);

        srs::persistence::CourseRepository courses(database);

        REQUIRE(courses.save({
            "CS201",
            "Data Structures",
            4,
            "faculty01",
            true
        }));

        const auto course = courses.find("CS201");

        REQUIRE(course);
        REQUIRE(course.value()->code == "CS201");
        REQUIRE(course.value()->title == "Data Structures");
        REQUIRE(course.value()->credits == 4);
        REQUIRE(course.value()->faculty_username == "faculty01");
        REQUIRE(course.value()->active);
    }

    clean_database();
}

TEST_CASE("Duplicate course codes update the existing course", "[course-management]") {
    clean_database();

    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());

        seed_required_records(database);

        srs::persistence::CourseRepository courses(database);

        REQUIRE(courses.save({
            "CS201",
            "Data Structures",
            4,
            "faculty01",
            true
        }));

        REQUIRE(courses.save({
            "CS201",
            "Advanced Data Structures",
            3,
            "faculty01",
            true
        }));

        const auto course = courses.find("CS201");

        REQUIRE(course);
        REQUIRE(course.value()->title == "Advanced Data Structures");
        REQUIRE(course.value()->credits == 3);

        const auto all_courses = courses.all();

        REQUIRE(all_courses);
        REQUIRE(all_courses.value()->size() == 1);
    }

    clean_database();
}

TEST_CASE("Course can be deactivated", "[course-management]") {
    clean_database();

    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());

        seed_required_records(database);

        srs::persistence::CourseRepository courses(database);

        REQUIRE(courses.save({
            "CS201",
            "Data Structures",
            4,
            "faculty01",
            true
        }));

        REQUIRE(courses.deactivate("CS201"));

        const auto course = courses.find("CS201");

        REQUIRE(course);
        REQUIRE_FALSE(course.value()->active);
    }

    clean_database();
}

TEST_CASE("Enrollment can be created and retrieved", "[course-management]") {
    clean_database();

    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());

        seed_required_records(database);

        srs::persistence::CourseRepository courses(database);
        srs::persistence::EnrollmentRepository enrollments(database);

        REQUIRE(courses.save({
            "CS201",
            "Data Structures",
            4,
            "faculty01",
            true
        }));

        REQUIRE(enrollments.save({
            "SRN-001",
            "CS201",
            3
        }));

        const auto all_enrollments = enrollments.all();

        REQUIRE(all_enrollments);
        REQUIRE(all_enrollments.value()->size() == 1);
        REQUIRE(all_enrollments.value()->front().srn == "SRN-001");
        REQUIRE(all_enrollments.value()->front().course_code == "CS201");
        REQUIRE(all_enrollments.value()->front().semester == 3);
    }

    clean_database();
}

TEST_CASE("Duplicate enrollment is rejected", "[course-management]") {
    clean_database();

    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());

        seed_required_records(database);

        srs::persistence::CourseRepository courses(database);
        srs::persistence::EnrollmentRepository enrollments(database);

        REQUIRE(courses.save({
            "CS201",
            "Data Structures",
            4,
            "faculty01",
            true
        }));

        REQUIRE(enrollments.save({
            "SRN-001",
            "CS201",
            3
        }));

        REQUIRE_FALSE(enrollments.save({
            "SRN-001",
            "CS201",
            3
        }));
    }

    clean_database();
}

TEST_CASE("Enrollment can be removed", "[course-management]") {
    clean_database();

    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());

        seed_required_records(database);

        srs::persistence::CourseRepository courses(database);
        srs::persistence::EnrollmentRepository enrollments(database);

        REQUIRE(courses.save({
            "CS201",
            "Data Structures",
            4,
            "faculty01",
            true
        }));

        REQUIRE(enrollments.save({
            "SRN-001",
            "CS201",
            3
        }));

        REQUIRE(enrollments.remove("SRN-001", "CS201", 3));

        const auto all_enrollments = enrollments.all();

        REQUIRE(all_enrollments);
        REQUIRE(all_enrollments.value()->empty());
    }

    clean_database();
}

TEST_CASE("Course and enrollment records persist after reopening database",
          "[course-management][persistence]") {
    clean_database();

    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());

        seed_required_records(database);

        srs::persistence::CourseRepository courses(database);
        srs::persistence::EnrollmentRepository enrollments(database);

        REQUIRE(courses.save({
            "CS201",
            "Data Structures",
            4,
            "faculty01",
            true
        }));

        REQUIRE(enrollments.save({
            "SRN-001",
            "CS201",
            3
        }));
    }

    {
        srs::persistence::Database database(test_database);
        REQUIRE(database.is_open());

        srs::persistence::CourseRepository courses(database);
        srs::persistence::EnrollmentRepository enrollments(database);

        const auto course = courses.find("CS201");
        REQUIRE(course);
        REQUIRE(course.value()->title == "Data Structures");

        const auto all_enrollments = enrollments.all();
        REQUIRE(all_enrollments);
        REQUIRE(all_enrollments.value()->size() == 1);
        REQUIRE(all_enrollments.value()->front().srn == "SRN-001");
    }

    clean_database();
}
