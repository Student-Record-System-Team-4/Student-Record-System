#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "course/course_service.h"
#include "grades/grade_service.h"
#include "student/student_profiles.h"

using srs::auth::AuthenticationService;
using srs::auth::Role;
using srs::auth::User;
using srs::auth::UserAccountService;
using srs::core::ErrorCode;
using srs::course::Course;
using srs::course::CourseService;
using srs::grades::GradeScale;
using srs::grades::GradeService;
using srs::student::StudentProfile;
using srs::student::StudentProfileService;
using srs::student::UserRole;

namespace {

// admin; faculty01 teaches CS201 (4cr) and CS203 (2cr); faculty02 teaches CS202 (3cr).
// Alice (SRN-001) is enrolled in CS201+CS202 in semester 3 and CS203 in semester 4.
// Bob (SRN-002) is enrolled in CS201 in semester 3 only.
struct Fixture {
    AuthenticationService auth{std::vector<User>{
        {"admin01", "Admin1234", Role::Admin},
        {"faculty01", "Teach1234", Role::Faculty},
        {"faculty02", "Teach5678", Role::Faculty},
        {"student01", "Pass1234", Role::Student},
        {"student02", "Pass5678", Role::Student}}};
    UserAccountService accounts{auth};
    StudentProfileService students{auth};
    CourseService courses{accounts, auth, students};
    GradeService grades{accounts, auth, students, courses};

    std::string admin, faculty1, faculty2, alice, bob;

    std::string login(const std::string& u, const std::string& p) {
        const auto s = auth.login(u, p);
        REQUIRE(s != nullptr);
        return s->id;
    }

    Fixture() {
        REQUIRE(students.register_student(
            StudentProfile{"student01", "Alice Smith", "SRN-001", "2005-01-01",
                           "555-0100", "Computer Science", 3, true},
            UserRole::Admin));
        REQUIRE(students.register_student(
            StudentProfile{"student02", "Bob Jones", "SRN-002", "2005-02-02",
                           "555-0101", "Computer Science", 3, true},
            UserRole::Admin));
        admin = login("admin01", "Admin1234");
        faculty1 = login("faculty01", "Teach1234");
        faculty2 = login("faculty02", "Teach5678");
        alice = login("student01", "Pass1234");
        bob = login("student02", "Pass5678");

        REQUIRE(courses.create_course(admin, Course{"CS201", "Data Structures", 4, "faculty01"}));
        REQUIRE(courses.create_course(admin, Course{"CS202", "Databases", 3, "faculty02"}));
        REQUIRE(courses.create_course(admin, Course{"CS203", "Networks", 2, "faculty01"}));
        REQUIRE(courses.enroll_student(admin, "SRN-001", "CS201", 3));
        REQUIRE(courses.enroll_student(admin, "SRN-001", "CS202", 3));
        REQUIRE(courses.enroll_student(admin, "SRN-001", "CS203", 4));
        REQUIRE(courses.enroll_student(admin, "SRN-002", "CS201", 3));
    }
};

}  // namespace

TEST_CASE("Marks map to letters and grade points", "[grades][scale]") {
    REQUIRE(GradeScale::letter_for_marks(100) == "S");
    REQUIRE(GradeScale::letter_for_marks(90) == "S");
    REQUIRE(GradeScale::letter_for_marks(89.99) == "A");
    REQUIRE(GradeScale::letter_for_marks(80) == "A");
    REQUIRE(GradeScale::letter_for_marks(70) == "B");
    REQUIRE(GradeScale::letter_for_marks(60) == "C");
    REQUIRE(GradeScale::letter_for_marks(50) == "D");
    REQUIRE(GradeScale::letter_for_marks(40) == "E");
    REQUIRE(GradeScale::letter_for_marks(39.9) == "F");
    REQUIRE(GradeScale::letter_for_marks(0) == "F");

    REQUIRE(*GradeScale::points_for_letter("S").value() == 10.0);
    REQUIRE(*GradeScale::points_for_letter("a").value() == 9.0);
    REQUIRE(*GradeScale::points_for_letter("F").value() == 0.0);
    REQUIRE_FALSE(GradeScale::points_for_letter("Z"));
    REQUIRE_FALSE(GradeScale::points_for_letter(""));
    REQUIRE_FALSE(GradeScale::points_for_letter("AA"));
}

TEST_CASE("Assigned faculty can enter marks; student and admin can see them", "[grades]") {
    Fixture f;
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 3, 92.5));

    const auto mine = f.grades.view_my_grades(f.alice);
    REQUIRE(mine);
    REQUIRE(mine.value()->size() == 1);
    const auto& record = mine.value()->front();
    REQUIRE(record.course_code == "CS201");
    REQUIRE(record.course_title == "Data Structures");
    REQUIRE(record.semester == 3);
    REQUIRE(record.credits == 4);
    REQUIRE(record.has_marks);
    REQUIRE(record.marks == 92.5);
    REQUIRE(record.letter == "S");
    REQUIRE(record.grade_points == 10.0);

    const auto as_admin = f.grades.view_student_grades(f.admin, "SRN-001");
    REQUIRE(as_admin);
    REQUIRE(as_admin.value()->size() == 1);
    REQUIRE(as_admin.value()->front().letter == "S");
}

TEST_CASE("Letter grades can be entered directly", "[grades]") {
    Fixture f;
    REQUIRE(f.grades.enter_letter_grade(f.faculty1, "SRN-001", "CS201", 3, "b"));

    const auto record = f.grades.view_my_grades(f.alice).value()->front();
    REQUIRE_FALSE(record.has_marks);
    REQUIRE(record.letter == "B");
    REQUIRE(record.grade_points == 8.0);

    REQUIRE(f.grades.enter_letter_grade(f.faculty1, "SRN-001", "CS201", 3, "Z").error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.grades.view_my_grades(f.alice).value()->front().letter == "B");  // unchanged
}

TEST_CASE("Invalid marks are rejected", "[grades]") {
    Fixture f;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (double bad : {-0.1, 100.1, nan}) {
        REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 3, bad).error() ==
                ErrorCode::INVALID_INPUT);
    }
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 0, 80).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 13, 80).error() ==
            ErrorCode::INVALID_INPUT);
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 3, 0));
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-002", "CS201", 3, 100));
    REQUIRE(f.grades.view_my_grades(f.alice).value()->size() == 1);
}

TEST_CASE("Entering a grade again updates it", "[grades]") {
    Fixture f;
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 3, 72));
    REQUIRE(f.grades.view_my_grades(f.alice).value()->front().letter == "B");

    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 3, 95));
    const auto grades = f.grades.view_my_grades(f.alice);
    REQUIRE(grades.value()->size() == 1);
    REQUIRE(grades.value()->front().letter == "S");
    REQUIRE(grades.value()->front().marks == 95.0);
}

TEST_CASE("Only the assigned faculty may grade a course", "[grades][rbac]") {
    Fixture f;
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 3, 85));

    // Other faculty (F-016 / TC-Grd-04): denied and the grade is untouched.
    auto other = f.grades.enter_marks(f.faculty2, "SRN-001", "CS201", 3, 10);
    REQUIRE_FALSE(other);
    REQUIRE(other.error() == ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.grades.enter_letter_grade(f.faculty2, "SRN-001", "CS201", 3, "F").error() ==
            ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.grades.view_my_grades(f.alice).value()->front().marks == 85.0);

    // Students and admins cannot enter grades at all.
    REQUIRE(f.grades.enter_marks(f.alice, "SRN-001", "CS201", 3, 100).error() ==
            ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.grades.enter_marks(f.admin, "SRN-001", "CS201", 3, 100).error() ==
            ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.grades.enter_marks("forged", "SRN-001", "CS201", 3, 100).error() ==
            ErrorCode::SESSION_ERROR);

    // Unknown course: nobody is assigned to it.
    REQUIRE_FALSE(f.grades.enter_marks(f.faculty1, "SRN-001", "NOPE", 3, 80));
}

TEST_CASE("Grades require enrolment in that course and semester", "[grades]") {
    Fixture f;
    auto not_enrolled = f.grades.enter_marks(f.faculty1, "SRN-002", "CS203", 4, 80);
    REQUIRE_FALSE(not_enrolled);
    REQUIRE(not_enrolled.error() == ErrorCode::NOT_FOUND);

    // Alice is enrolled in CS201 for semester 3, not 4.
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 4, 80).error() ==
            ErrorCode::NOT_FOUND);
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-999", "CS201", 3, 80).error() ==
            ErrorCode::NOT_FOUND);
    REQUIRE(f.grades.view_student_grades(f.admin, "SRN-002").value()->empty());
}

TEST_CASE("GPA and CGPA are credit-weighted", "[grades][gpa]") {
    Fixture f;
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 3, 95));       // S=10, 4cr
    REQUIRE(f.grades.enter_letter_grade(f.faculty2, "SRN-001", "CS202", 3, "B"));  // 8, 3cr
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS203", 4, 65));       // C=7, 2cr

    const auto summary = f.grades.view_my_summary(f.alice);
    REQUIRE(summary);
    REQUIRE(summary.value()->semesters.size() == 2);

    const auto& sem3 = summary.value()->semesters[0];
    REQUIRE(sem3.semester == 3);
    REQUIRE(sem3.credits == 7);
    REQUIRE(sem3.gpa == 9.14);  // (10*4 + 8*3) / 7 = 9.142857...

    const auto& sem4 = summary.value()->semesters[1];
    REQUIRE(sem4.semester == 4);
    REQUIRE(sem4.gpa == 7.0);

    REQUIRE(summary.value()->total_credits == 9);
    REQUIRE(summary.value()->cgpa == 8.67);  // (40 + 24 + 14) / 9 = 8.666...

    const auto as_admin = f.grades.view_student_summary(f.admin, "SRN-001");
    REQUIRE(as_admin);
    REQUIRE(as_admin.value()->cgpa == 8.67);
}

TEST_CASE("A failing grade counts its credits at zero points", "[grades][gpa]") {
    Fixture f;
    REQUIRE(f.grades.enter_letter_grade(f.faculty1, "SRN-001", "CS201", 3, "S"));  // 10, 4cr
    REQUIRE(f.grades.enter_letter_grade(f.faculty2, "SRN-001", "CS202", 3, "F"));  // 0, 3cr

    const auto summary = f.grades.compute_summary("SRN-001");
    REQUIRE(summary.semesters.size() == 1);
    REQUIRE(summary.semesters[0].gpa == 5.71);  // 40 / 7
    REQUIRE(summary.cgpa == 5.71);
}

TEST_CASE("GPA follows grade updates and course credit changes", "[grades][gpa]") {
    Fixture f;
    REQUIRE(f.grades.enter_letter_grade(f.faculty1, "SRN-001", "CS201", 3, "S"));
    REQUIRE(f.grades.enter_letter_grade(f.faculty2, "SRN-001", "CS202", 3, "C"));
    REQUIRE(f.grades.compute_summary("SRN-001").cgpa == 8.71);  // (40 + 21) / 7

    REQUIRE(f.grades.enter_letter_grade(f.faculty2, "SRN-001", "CS202", 3, "S"));
    REQUIRE(f.grades.compute_summary("SRN-001").cgpa == 10.0);

    REQUIRE(f.grades.enter_letter_grade(f.faculty2, "SRN-001", "CS202", 3, "F"));
    REQUIRE(f.courses.update_course(
        f.admin, Course{"CS202", "Databases", 6, "faculty02"}));
    REQUIRE(f.grades.compute_summary("SRN-001").cgpa == 4.0);  // 40 / 10
}

TEST_CASE("A student with no grades has an empty summary", "[grades][gpa]") {
    Fixture f;
    const auto summary = f.grades.view_my_summary(f.bob);
    REQUIRE(summary);
    REQUIRE(summary.value()->semesters.empty());
    REQUIRE(summary.value()->total_credits == 0);
    REQUIRE(summary.value()->cgpa == 0.0);
    REQUIRE(f.grades.view_my_grades(f.bob).value()->empty());
}

TEST_CASE("Students only see their own grades", "[grades][rbac]") {
    Fixture f;
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-001", "CS201", 3, 95));
    REQUIRE(f.grades.enter_marks(f.faculty1, "SRN-002", "CS201", 3, 55));

    REQUIRE(f.grades.view_my_grades(f.alice).value()->front().marks == 95.0);
    REQUIRE(f.grades.view_my_grades(f.bob).value()->front().marks == 55.0);
    REQUIRE(f.grades.view_my_summary(f.bob).value()->cgpa == 6.0);

    // No way to ask for someone else's data through the student calls, and the
    // admin-only calls refuse students and faculty.
    REQUIRE(f.grades.view_student_grades(f.alice, "SRN-002").error() ==
            ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.grades.view_student_summary(f.alice, "SRN-002").error() ==
            ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.grades.view_student_grades(f.faculty1, "SRN-002").error() ==
            ErrorCode::PERMISSION_DENIED);

    // Faculty and admin have no "own grades"; logged-out sessions are refused.
    REQUIRE(f.grades.view_my_grades(f.faculty1).error() == ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.grades.view_my_grades(f.admin).error() == ErrorCode::PERMISSION_DENIED);
    REQUIRE(f.grades.view_my_grades("forged").error() == ErrorCode::SESSION_ERROR);
    REQUIRE(f.grades.view_student_grades("forged", "SRN-001").error() ==
            ErrorCode::SESSION_ERROR);
}
