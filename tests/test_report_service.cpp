
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "attendance/attendance_service.h"
#include "course/course_service.h"
#include "grades/grade_service.h"
#include "reports/report_service.h"
#include "student/student_profiles.h"

using srs::auth::AuthenticationService;
using srs::auth::Role;
using srs::auth::User;
using srs::auth::UserAccountService;
using srs::attendance::AttendanceService;
using srs::attendance::AttendanceStatus;
using srs::course::Course;
using srs::course::CourseService;
using srs::grades::GradeService;
using srs::reports::ReportService;
using srs::student::StudentProfile;
using srs::student::StudentProfileService;
using srs::core::ErrorCode;

namespace {

// ---------------------------------------------------------
// TEST FIXTURE
// ---------------------------------------------------------

struct ReportFixture {

    AuthenticationService auth{std::vector<User>{
        {"admin01", "Admin1234", Role::Admin},
        {"faculty01", "Teach1234", Role::Faculty},
        {"faculty02", "Teach5678", Role::Faculty},
        {"student01", "Pass1234", Role::Student},
        {"student02", "Pass5678", Role::Student}
    }};

    UserAccountService accounts{auth};
    StudentProfileService students{auth};
    CourseService courses{accounts, auth, students};
    GradeService grades{accounts, auth, students, courses};

    AttendanceService attendance{
        accounts, auth, students, courses
    };

    ReportService reports{
        accounts,
        auth,
        students,
        courses,
        grades,
        attendance
    };

    std::string admin;
    std::string faculty1;
    std::string faculty2;
    std::string alice;
    std::string bob;

    std::string login(
        const std::string& username,
        const std::string& password) {

        const auto session = auth.login(username, password);

        REQUIRE(session != nullptr);

        return session->id;
    }

    ReportFixture() {

        admin = login("admin01", "Admin1234");

        REQUIRE(students.register_student(
            admin,
            StudentProfile{
                "student01",
                "Alice Smith",
                "SRN-001",
                "2005-01-01",
                "555-0100",
                "Computer Science",
                3,
                true
            }));

        REQUIRE(students.register_student(
            admin,
            StudentProfile{
                "student02",
                "Bob Jones",
                "SRN-002",
                "2005-02-02",
                "555-0101",
                "Computer Science",
                3,
                true
            }));

        faculty1 = login("faculty01", "Teach1234");
        faculty2 = login("faculty02", "Teach5678");

        alice = login("student01", "Pass1234");
        bob = login("student02", "Pass5678");

        REQUIRE(courses.create_course(
            admin,
            Course{"CS201", "Data Structures", 4, "faculty01"}));

        REQUIRE(courses.create_course(
            admin,
            Course{"CS202", "Databases", 3, "faculty02"}));

        REQUIRE(courses.create_course(
            admin,
            Course{"CS203", "Networks", 2, "faculty01"}));

        REQUIRE(courses.enroll_student(
            admin, "SRN-001", "CS201", 3));

        REQUIRE(courses.enroll_student(
            admin, "SRN-001", "CS202", 3));

        REQUIRE(courses.enroll_student(
            admin, "SRN-001", "CS203", 4));

        REQUIRE(courses.enroll_student(
            admin, "SRN-002", "CS201", 3));
    }
};

// ---------------------------------------------------------
// FILE READING HELPER
// ---------------------------------------------------------

std::string read_exported_file(const std::string& file_path) {

    std::ifstream file(file_path);

    if (!file.is_open()) {
        return "";
    }

    std::ostringstream contents;
    contents << file.rdbuf();

    return contents.str();
}

} // namespace

// ---------------------------------------------------------
// STUDENT REPORT TESTS
// ---------------------------------------------------------

TEST_CASE(
    "Student can generate their own academic report",
    "[reports][student]") {

    ReportFixture f;

    REQUIRE(f.grades.enter_marks(
        f.faculty1, "SRN-001", "CS201", 3, 95));

    const auto report =
        f.reports.generate_my_report(f.alice);

    REQUIRE(report);

    REQUIRE(report.value()->profile.srn == "SRN-001");
    REQUIRE(report.value()->profile.name == "Alice Smith");

    REQUIRE(report.value()->grades.size() == 1);

    REQUIRE(
        report.value()->grades.front().course_code == "CS201");

    REQUIRE(
        report.value()->academic_summary.total_credits == 4);

    REQUIRE(
        report.value()->academic_summary.cgpa == 10.0);

    REQUIRE(report.value()->attendance.size() == 3);
}

TEST_CASE(
    "Student cannot generate another student's report",
    "[reports][rbac]") {

    ReportFixture f;

    const auto result =
        f.reports.generate_student_report(
            f.alice,
            "SRN-002");

    REQUIRE_FALSE(result);

    REQUIRE(
        result.error() == ErrorCode::PERMISSION_DENIED);
}

TEST_CASE(
    "Student cannot generate a report using another SRN",
    "[reports][rbac]") {

    ReportFixture f;

    const auto result =
        f.reports.generate_my_report("forged");

    REQUIRE_FALSE(result);

    REQUIRE(
        result.error() == ErrorCode::SESSION_ERROR);
}

// ---------------------------------------------------------
// ADMIN REPORT TESTS
// ---------------------------------------------------------

TEST_CASE(
    "Admin can generate another student's academic report",
    "[reports][admin]") {

    ReportFixture f;

    REQUIRE(f.grades.enter_marks(
        f.faculty1, "SRN-001", "CS201", 3, 95));

    REQUIRE(f.grades.enter_letter_grade(
        f.faculty2, "SRN-001", "CS202", 3, "B"));

    const auto report =
        f.reports.generate_student_report(
            f.admin,
            "SRN-001");

    REQUIRE(report);

    REQUIRE(report.value()->profile.srn == "SRN-001");

    REQUIRE(report.value()->grades.size() == 2);

    REQUIRE(
        report.value()->academic_summary.total_credits == 7);

    REQUIRE(
        report.value()->academic_summary.cgpa == 9.14);
}

TEST_CASE(
    "Admin cannot generate a report for an unknown student",
    "[reports][admin]") {

    ReportFixture f;

    const auto report =
        f.reports.generate_student_report(
            f.admin,
            "SRN-999");

    REQUIRE_FALSE(report);

    REQUIRE(report.error() == ErrorCode::NOT_FOUND);
}

TEST_CASE(
    "Faculty cannot generate institution-level student reports",
    "[reports][rbac]") {

    ReportFixture f;

    const auto report =
        f.reports.generate_student_report(
            f.faculty1,
            "SRN-001");

    REQUIRE_FALSE(report);

    REQUIRE(
        report.error() == ErrorCode::PERMISSION_DENIED);
}

// ---------------------------------------------------------
// COURSE PERFORMANCE REPORT TESTS
// ---------------------------------------------------------

TEST_CASE(
    "Course report calculates grade statistics correctly",
    "[reports][course]") {

    ReportFixture f;

    REQUIRE(f.grades.enter_marks(
        f.faculty1,
        "SRN-001",
        "CS201",
        3,
        95));

    REQUIRE(f.grades.enter_letter_grade(
        f.faculty1,
        "SRN-002",
        "CS201",
        3,
        "B"));

    const auto report =
        f.reports.generate_course_report(
            f.admin,
            "CS201",
            3);

    REQUIRE(report);

    REQUIRE(report.value()->course_code == "CS201");

    REQUIRE(
        report.value()->course_title == "Data Structures");

    REQUIRE(report.value()->semester == 3);

    REQUIRE(report.value()->enrolled_students == 2);

    REQUIRE(report.value()->graded_students == 2);

    REQUIRE(
        report.value()->average_grade_points == 9.0);

    REQUIRE(
        report.value()->highest_grade_points == 10.0);

    REQUIRE(
        report.value()->lowest_grade_points == 8.0);
}

TEST_CASE(
    "Course report includes letter-only grades",
    "[reports][course][grades]") {

    ReportFixture f;

    REQUIRE(f.grades.enter_letter_grade(
        f.faculty1,
        "SRN-001",
        "CS201",
        3,
        "A"));

    const auto report =
        f.reports.generate_course_report(
            f.admin,
            "CS201",
            3);

    REQUIRE(report);

    REQUIRE(report.value()->graded_students == 1);

    REQUIRE(
        report.value()->average_grade_points == 9.0);

    REQUIRE(
        report.value()->highest_grade_points == 9.0);

    REQUIRE(
        report.value()->lowest_grade_points == 9.0);
}

TEST_CASE(
    "Course report ignores students without grades",
    "[reports][course]") {

    ReportFixture f;

    REQUIRE(f.grades.enter_marks(
        f.faculty1,
        "SRN-001",
        "CS201",
        3,
        85));

    const auto report =
        f.reports.generate_course_report(
            f.admin,
            "CS201",
            3);

    REQUIRE(report);

    REQUIRE(report.value()->enrolled_students == 2);

    REQUIRE(report.value()->graded_students == 1);

    REQUIRE(
        report.value()->average_grade_points == 9.0);
}

TEST_CASE(
    "Course report calculates attendance correctly",
    "[reports][attendance]") {

    ReportFixture f;

    REQUIRE(f.attendance.mark_attendance(
        f.faculty1,
        "SRN-001",
        "CS201",
        3,
        "2026-11-01",
        "Lecture 1",
        AttendanceStatus::Present));

    REQUIRE(f.attendance.mark_attendance(
        f.faculty1,
        "SRN-002",
        "CS201",
        3,
        "2026-11-01",
        "Lecture 1",
        AttendanceStatus::Absent));

    const auto report =
        f.reports.generate_course_report(
            f.admin,
            "CS201",
            3);

    REQUIRE(report);

    REQUIRE(
        report.value()->average_attendance == 50.0);
}

// ---------------------------------------------------------
// VALIDATION AND PERMISSION TESTS
// ---------------------------------------------------------

TEST_CASE(
    "Course report rejects invalid input",
    "[reports][validation]") {

    ReportFixture f;

    REQUIRE(
        f.reports.generate_course_report(
            f.admin,
            "",
            3).error() == ErrorCode::INVALID_INPUT);

    REQUIRE(
        f.reports.generate_course_report(
            f.admin,
            "CS201",
            -1).error() == ErrorCode::INVALID_INPUT);
}

TEST_CASE(
    "Course report rejects unknown courses",
    "[reports][validation]") {

    ReportFixture f;

    const auto report =
        f.reports.generate_course_report(
            f.admin,
            "UNKNOWN",
            3);

    REQUIRE_FALSE(report);

    REQUIRE(report.error() == ErrorCode::NOT_FOUND);
}

TEST_CASE(
    "Students cannot generate course performance reports",
    "[reports][rbac]") {

    ReportFixture f;

    const auto report =
        f.reports.generate_course_report(
            f.alice,
            "CS201",
            3);

    REQUIRE_FALSE(report);

    REQUIRE(
        report.error() == ErrorCode::PERMISSION_DENIED);
}

TEST_CASE(
    "Invalid session cannot generate a student report",
    "[reports][rbac]") {

    ReportFixture f;

    const auto report =
        f.reports.generate_student_report(
            "forged",
            "SRN-001");

    REQUIRE_FALSE(report);

    REQUIRE(
        report.error() == ErrorCode::SESSION_ERROR);
}

// ---------------------------------------------------------
// CSV EXPORT TESTS
// ---------------------------------------------------------

TEST_CASE(
    "Student can export their academic report as CSV",
    "[reports][export][csv]") {

    ReportFixture f;

    const std::string file_path = "student_report_test.csv";

    std::remove(file_path.c_str());

    REQUIRE(f.grades.enter_marks(
        f.faculty1,
        "SRN-001",
        "CS201",
        3,
        95));

    REQUIRE(f.attendance.mark_attendance(
        f.faculty1,
        "SRN-001",
        "CS201",
        3,
        "2026-11-01",
        "Lecture 1",
        AttendanceStatus::Present));

    const auto result =
        f.reports.export_student_report_csv(
            f.alice,
            "SRN-001",
            file_path);

    REQUIRE(result);

    const std::string contents =
        read_exported_file(file_path);

    REQUIRE_FALSE(contents.empty());

    REQUIRE(
        contents.find("Alice Smith") != std::string::npos);

    REQUIRE(
        contents.find("SRN-001") != std::string::npos);

    REQUIRE(
        contents.find("CS201") != std::string::npos);

    std::remove(file_path.c_str());
}

TEST_CASE(
    "CSV export rejects an invalid file path",
    "[reports][export][csv][validation]") {

    ReportFixture f;

    const auto result =
        f.reports.export_student_report_csv(
            f.alice,
            "SRN-001",
            "missing_reports_directory/report.csv");

    REQUIRE_FALSE(result);
}

// ---------------------------------------------------------
// TEXT EXPORT TESTS
// ---------------------------------------------------------

TEST_CASE(
    "Student can export their academic report as text",
    "[reports][export][text]") {

    ReportFixture f;

    const std::string file_path = "student_report_test.txt";

    std::remove(file_path.c_str());

    REQUIRE(f.grades.enter_marks(
        f.faculty1,
        "SRN-001",
        "CS201",
        3,
        95));

    REQUIRE(f.attendance.mark_attendance(
        f.faculty1,
        "SRN-001",
        "CS201",
        3,
        "2026-11-01",
        "Lecture 1",
        AttendanceStatus::Present));

    const auto result =
        f.reports.export_student_report_text(
            f.alice,
            "SRN-001",
            file_path);

    REQUIRE(result);

    const std::string contents =
        read_exported_file(file_path);

    REQUIRE_FALSE(contents.empty());

    REQUIRE(
        contents.find("Alice Smith") != std::string::npos);

    REQUIRE(
        contents.find("SRN-001") != std::string::npos);

    REQUIRE(
        contents.find("CS201") != std::string::npos);

    std::remove(file_path.c_str());
}

TEST_CASE(
    "Text export rejects an invalid file path",
    "[reports][export][text][validation]") {

    ReportFixture f;

    const auto result =
        f.reports.export_student_report_text(
            f.alice,
            "SRN-001",
            "missing_reports_directory/report.txt");

    REQUIRE_FALSE(result);
}
