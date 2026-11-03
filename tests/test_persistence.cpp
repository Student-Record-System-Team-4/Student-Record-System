#include <cstdio>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "persistence/database.h"
#include "persistence/repositories.h"

using namespace srs::persistence;

TEST_CASE("Grades persist and reload after reopening SQLite database",
          "[persistence][grades]") {
    const char* db_path = "grade_persistence_test.db";
    std::remove(db_path);

    // Create the database and save the related records.
    {
        Database database(db_path);
        REQUIRE(database.is_open());

        UserRepository users(database);
        StudentRepository students(database);
        CourseRepository courses(database);
        EnrollmentRepository enrollments;
        GradeRepository grades(database);

        REQUIRE(users.save(UserRow{
            "student1", "hashed_password", "student", true
        }));

        REQUIRE(students.save(StudentRow{
            "SRN-001",
            "student1",
            "Test Student",
            "2005-01-01",
            "9876543210",
            "CSE",
            3,
            true
        }));

        REQUIRE(courses.save(CourseRow{
            "CS101",
            "Introduction to Programming",
            4,
            "faculty1"
        }));

        REQUIRE(enrollments.save(EnrollmentRow{
            "SRN-001",
            "CS101",
            3
        }));

        REQUIRE(grades.save(GradeRow{
            "SRN-001",
            "CS101",
            "A",
            3,
            true,
            85.0,
            9.0
        }));
    }

    // Reopen the database and verify that the grade was persisted.
    {
        Database database(db_path);
        REQUIRE(database.is_open());

        GradeRepository grades(database);
        auto result = grades.all();

        REQUIRE(result);
        REQUIRE(result.value().size() == 1);

        const GradeRow& grade = result.value().front();

        CHECK(grade.srn == "SRN-001");
        CHECK(grade.course_code == "CS101");
        CHECK(grade.letter == "A");
        CHECK(grade.semester == 3);
        CHECK(grade.has_marks);
        CHECK(grade.marks == 85.0);
        CHECK(grade.grade_points == 9.0);
    }

    std::remove(db_path);
}
