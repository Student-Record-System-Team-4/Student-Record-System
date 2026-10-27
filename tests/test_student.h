
#include <catch2/catch_test_macros.hpp>

#include "student/student.h"

using student_record::Student;
using student_record::StudentManager;

TEST_CASE("Register a valid student") {
    StudentManager manager;

    Student student{
        "Abhay Babji P",
        "PES2UG24CS342",
        "2005-01-01",
        "9876543210",
        "CSE",
        5,
        true
    };

    REQUIRE(manager.register_student(student));

    const auto* profile = manager.get_own_profile("PES2UG24CS342");
    REQUIRE(profile != nullptr);
    CHECK(profile->name == "Abhay Babji P");
}

TEST_CASE("Reject duplicate student SRN") {
    StudentManager manager;

    Student student{
        "Student One", "SRN001", "2005-01-01",
        "9876543210", "CSE", 3, true
    };

    REQUIRE(manager.register_student(student));
    CHECK_FALSE(manager.register_student(student));
}

TEST_CASE("Reject invalid student data") {
    StudentManager manager;

    Student student{
        "   ", "SRN002", "2005-01-01",
        "9876543210", "CSE", 3, true
    };

    CHECK_FALSE(manager.register_student(student));

    student.name = "Student Two";
    student.semester = 0;

    CHECK_FALSE(manager.register_student(student));
}

TEST_CASE("Update a student profile") {
    StudentManager manager;

    Student student{
        "Student One", "SRN003", "2005-01-01",
        "9876543210", "CSE", 3, true
    };

    REQUIRE(manager.register_student(student));

    student.name = "Updated Student";
    student.semester = 4;

    REQUIRE(manager.update_student("SRN003", student));

    const auto* profile = manager.get_own_profile("SRN003");
    REQUIRE(profile != nullptr);
    CHECK(profile->name == "Updated Student");
    CHECK(profile->semester == 4);
}

TEST_CASE("Deactivate a student") {
    StudentManager manager;

    Student student{
        "Student One", "SRN004", "2005-01-01",
        "9876543210", "CSE", 3, true
    };

    REQUIRE(manager.register_student(student));
    REQUIRE(manager.deactivate_student("SRN004"));

    CHECK(manager.get_own_profile("SRN004") == nullptr);
    CHECK(manager.search_students().empty());
}

TEST_CASE("Student can access only their own profile by SRN") {
    StudentManager manager;

    REQUIRE(manager.register_student(Student{
        "Student One", "SRN005", "2005-01-01",
        "9876543210", "CSE", 3, true
    }));

    REQUIRE(manager.register_student(Student{
        "Student Two", "SRN006", "2005-01-01",
        "9876543211", "ECE", 4, true
    }));

    CHECK(manager.get_own_profile("SRN005")->srn == "SRN005");
    CHECK(manager.get_own_profile("UNKNOWN") == nullptr);
}

TEST_CASE("Search students by name, SRN, programme, and semester") {
    StudentManager manager;

    REQUIRE(manager.register_student(Student{
        "Alice Smith", "SRN007", "2005-01-01",
        "9876543210", "CSE", 3, true
    }));

    REQUIRE(manager.register_student(Student{
        "Bob Jones", "SRN008", "2005-01-01",
        "9876543211", "ECE", 4, true
    }));

    CHECK(manager.search_students("alice").size() == 1);
    CHECK(manager.search_students("", "SRN008").size() == 1);
    CHECK(manager.search_students("", "", "cse").size() == 1);
    CHECK(manager.search_students("", "", "", 4).size() == 1);
}
