#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <vector>

#include "attendance/attendance_service.h"
#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "course/course_service.h"
#include "grades/grade_service.h"
#include "student/student_profiles.h"
#include "ui/cli.h"

// The CLI is driven with scripted input and its output is captured. The
// services behind it are the real ones (in memory), so these tests check both
// what the user sees and what actually ended up in the services.

using srs::auth::AuthenticationService;
using srs::auth::Role;
using srs::auth::User;
using srs::auth::UserAccountService;
using srs::course::Course;
using srs::course::CourseService;
using srs::grades::GradeService;
using srs::student::StudentProfile;
using srs::student::StudentProfileService;
using srs::attendance::AttendanceService;

namespace {

struct World {
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
    AttendanceService attendance{accounts, auth, students, courses};

    // Seeds data through the services (not through the CLI) for tests that
    // are about something else.
    void seed() {
        const auto admin = auth.login("admin01", "Admin1234")->id;
        REQUIRE(students.register_student(
            admin, StudentProfile{"student01", "Alice Smith", "SRN-001",
                                  "2005-01-01", "555-0100",
                                  "Computer Science", 3, true}));
        REQUIRE(students.register_student(
            admin, StudentProfile{"student02", "Bob Jones", "SRN-002",
                                  "2005-02-02", "555-0101",
                                  "Computer Science", 3, true}));
        REQUIRE(courses.create_course(
            admin, Course{"CS201", "Data Structures", 4, "faculty01"}));
        REQUIRE(courses.enroll_student(admin, "SRN-001", "CS201", 3));
        REQUIRE(courses.enroll_student(admin, "SRN-002", "CS201", 3));
        auth.logout(admin);
    }

    // Runs the CLI over the given input lines; returns everything it printed.
    std::string run(const std::vector<std::string>& lines, int* exit_code = nullptr) {
        std::string script;
        for (const auto& line : lines) {
            script += line + "\n";
        }
        std::istringstream in(script);
        std::ostringstream out;
        srs::ui::Cli cli({accounts, auth, students, courses, grades, attendance},
                         in, out);
        const int code = cli.run();
        if (exit_code) {
            *exit_code = code;
        }
        return out.str();
    }

    std::string session(const std::string& user, const std::string& password) {
        const auto s = auth.login(user, password);
        REQUIRE(s != nullptr);
        return s->id;
    }
};

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

}  // namespace

// --- login, logout, exit ---------------------------------------------------

TEST_CASE("The CLI starts, shows the login screen and exits cleanly",
          "[cli]") {
    World w;
    int code = -1;
    const auto output = w.run({"0"}, &code);

    REQUIRE(code == 0);
    REQUIRE(contains(output, "Student Record System"));
    REQUIRE(contains(output, "1. Log in"));
    REQUIRE(contains(output, "0. Exit"));
    REQUIRE(contains(output, "Goodbye."));
}

TEST_CASE("Login succeeds with valid credentials and logout returns to the login screen",
          "[cli]") {
    World w;
    const auto output = w.run({"1", "student01", "Pass1234", "0", "0"});

    REQUIRE(contains(output, "Welcome, student01 (Student)."));
    REQUIRE(contains(output, "You have been logged out."));
    REQUIRE(contains(output, "Goodbye."));
}

TEST_CASE("Failed logins are reported and do not log anyone in", "[cli]") {
    World w;
    const auto output = w.run({
        "1", "student01", "WrongPass1",   // wrong password
        "1", "nobody", "Whatever123",     // unknown user
        "1", "", "",                      // empty credentials
        "0"});

    REQUIRE(contains(output, "Login failed"));
    REQUIRE_FALSE(contains(output, "Welcome, "));
    REQUIRE_FALSE(contains(output, "Main menu"));
}

TEST_CASE("The q option logs out and exits from the main menu", "[cli]") {
    World w;
    const auto output = w.run({"1", "admin01", "Admin1234", "q"});

    REQUIRE(contains(output, "You have been logged out."));
    REQUIRE(contains(output, "Goodbye."));
}

TEST_CASE("Ending input while logged in logs out and exits without hanging",
          "[cli]") {
    World w;
    int code = -1;
    const auto output = w.run({"1", "admin01", "Admin1234"}, &code);

    REQUIRE(code == 0);
    REQUIRE(contains(output, "You have been logged out."));
    REQUIRE(contains(output, "Goodbye."));

    // Ending input in the middle of a prompt is also fine.
    REQUIRE(w.run({"1", "admin01"}, &code).size() > 0);
    REQUIRE(code == 0);
    REQUIRE(w.run({}, &code).size() > 0);
    REQUIRE(code == 0);
}

// --- role-specific menus ---------------------------------------------------

TEST_CASE("Menus depend on the user's role", "[cli][rbac]") {
    World w;

    const auto student = w.run({"1", "student01", "Pass1234", "q"});
    REQUIRE(contains(student, "Students"));
    REQUIRE(contains(student, "Grades"));
    REQUIRE(contains(student, "Attendance"));
    REQUIRE(contains(student, "Reports"));
    REQUIRE_FALSE(contains(student, "Courses and enrollment"));
    REQUIRE_FALSE(contains(student, "User management"));

    const auto faculty = w.run({"1", "faculty01", "Teach1234", "q"});
    REQUIRE(contains(faculty, "Courses and enrollment"));
    REQUIRE(contains(faculty, "Grades"));
    REQUIRE(contains(faculty, "Attendance"));
    REQUIRE_FALSE(contains(faculty, "Reports"));
    REQUIRE_FALSE(contains(faculty, "User management"));

    const auto admin = w.run({"1", "admin01", "Admin1234", "q"});
    REQUIRE(contains(admin, "Students"));
    REQUIRE(contains(admin, "Courses and enrollment"));
    REQUIRE(contains(admin, "User management"));
    REQUIRE(contains(admin, "Reports"));
}

TEST_CASE("Submenus only list the operations the role may use",
          "[cli][rbac]") {
    World w;

    // Faculty: Courses and enrollment is module 1.
    const auto faculty = w.run({"1", "faculty01", "Teach1234", "1", "0", "q"});
    REQUIRE(contains(faculty, "List courses"));
    REQUIRE(contains(faculty, "View a course roster"));
    REQUIRE_FALSE(contains(faculty, "Create a course"));
    REQUIRE_FALSE(contains(faculty, "Enroll a student in a course"));

    // Admin: Courses and enrollment is module 2.
    const auto admin = w.run({"1", "admin01", "Admin1234", "2", "0", "q"});
    REQUIRE(contains(admin, "Create a course"));
    REQUIRE(contains(admin, "Enroll a student in a course"));
    REQUIRE(contains(admin, "Remove a student from a course"));

    // Student: Grades is module 2; no grade entry.
    const auto student = w.run({"1", "student01", "Pass1234", "2", "0", "q"});
    REQUIRE(contains(student, "View my grades"));
    REQUIRE_FALSE(contains(student, "Enter marks"));
}

// --- invalid input ---------------------------------------------------------

TEST_CASE("Invalid menu input never crashes the CLI", "[cli][input]") {
    World w;
    const auto output = w.run({
        "abc", "-1", "99", "", "1.5", "9999999999999",  // login screen
        "1", "admin01", "Admin1234",
        "xyz", "99", "-3", "", "   ",                   // main menu
        "1", "foo", "42", "-1", "",                     // a submenu
        "0", "0", "0"});

    REQUIRE(contains(output, "Invalid choice"));
    REQUIRE(contains(output, "Goodbye."));
}

TEST_CASE("A non-numeric number cancels the operation and creates nothing",
          "[cli][input]") {
    World w;
    w.seed();

    // Admin > Courses > Create a course (item 2) with credits "four".
    const auto output = w.run({
        "1", "admin01", "Admin1234",
        "2", "2", "CS301", "Algorithms", "four",
        "0", "0", "0"});

    REQUIRE(contains(output, "Invalid number"));
    REQUIRE_FALSE(w.courses.get_course("CS301"));
}

TEST_CASE("Service errors are shown as messages, not crashes", "[cli][input]") {
    World w;
    w.seed();

    const auto output = w.run({
        "1", "admin01", "Admin1234",
        // Enroll an unknown student, then a duplicate, then a bad semester.
        "2", "5", "SRN-999", "CS201", "3",
        "5", "SRN-001", "CS201", "3",
        "5", "SRN-001", "CS201", "13",
        "0", "0", "0"});

    REQUIRE(contains(output, "Not found"));
    REQUIRE(contains(output, "already exists"));
    REQUIRE(contains(output, "Invalid input"));
}

// --- integration with the modules ------------------------------------------

TEST_CASE("An admin can set up users, students, courses and enrollments through the CLI",
          "[cli][admin]") {
    World w;

    w.run({
        "1", "admin01", "Admin1234",
        // User management (module 6): create a student login.
        "6", "1", "student09", "Pass9999", "Pass9999", "student", "0",
        // Students (module 1): register the profile (item 2).
        "1", "2", "student09", "Carol White", "SRN-009", "2005-03-03",
        "555-0109", "Computer Science", "3", "0",
        // Courses and enrollment (module 2): create (2), enroll (5).
        "2", "2", "CS301", "Algorithms", "3", "faculty01",
        "5", "SRN-009", "CS301", "3", "0",
        "0", "0"});

    REQUIRE(w.auth.is_registered("student09"));
    REQUIRE(w.courses.get_course("CS301"));
    REQUIRE(w.courses.get_course("CS301").value()->credits == 3);
    REQUIRE(w.courses.is_enrolled("SRN-009", "CS301", 3));
}

TEST_CASE("Admin can update, remove and deactivate through the CLI",
          "[cli][admin]") {
    World w;
    w.seed();

    w.run({
        "1", "admin01", "Admin1234",
        // Update the course: keep title, new credits, keep faculty.
        "2", "3", "CS201", "", "2", "",
        // Remove Bob from CS201 (item 6), confirmed.
        "6", "SRN-002", "CS201", "3", "YES",
        // Deactivate the course (item 4), confirmed.
        "4", "CS201", "YES",
        "0", "0", "0"});

    const auto course = w.courses.get_course("CS201");
    REQUIRE(course);
    REQUIRE(course.value()->credits == 2);
    REQUIRE(course.value()->title == "Data Structures");
    REQUIRE_FALSE(course.value()->active);
    REQUIRE_FALSE(w.courses.is_enrolled("SRN-002", "CS201", 3));
    REQUIRE(w.courses.is_enrolled("SRN-001", "CS201", 3));
}

TEST_CASE("Destructive actions need an explicit YES", "[cli][admin]") {
    World w;
    w.seed();

    w.run({
        "1", "admin01", "Admin1234",
        "2", "6", "SRN-002", "CS201", "3", "no",
        "4", "CS201", "yes",   // lower case is not a confirmation
        "0", "0", "0"});

    REQUIRE(w.courses.is_enrolled("SRN-002", "CS201", 3));
    REQUIRE(w.courses.get_course("CS201").value()->active);
}

TEST_CASE("Faculty enter grades and attendance; students see them",
          "[cli][grades][attendance]") {
    World w;
    w.seed();

    // Faculty: Grades is module 2 (marks = 1), Attendance is module 3.
    const auto faculty = w.run({
        "1", "faculty01", "Teach1234",
        "2", "1", "SRN-001", "CS201", "3", "85", "0",
        "3", "1", "SRN-001", "CS201", "3", "2026-11-01", "Lecture 1", "P",
        "1", "SRN-001", "CS201", "3", "2026-11-02", "Lecture 2", "A", "0",
        "0", "0"});
    REQUIRE(contains(faculty, "Marks saved."));
    REQUIRE(contains(faculty, "Attendance saved."));

    // Student: Grades is module 2 (my grades = 1, GPA = 2), Attendance is 3.
    const auto student = w.run({
        "1", "student01", "Pass1234",
        "2", "1", "2", "0",
        "3", "1", "CS201", "0",
        "0", "0"});
    REQUIRE(contains(student, "CS201"));
    REQUIRE(contains(student, "Data Structures"));
    REQUIRE(contains(student, "85.0"));
    REQUIRE(contains(student, "CGPA: 9.00 (4 credits)"));
    REQUIRE(contains(student, "Lecture 1"));
    REQUIRE(contains(student, "1 of 2 sessions present, 50.00%"));
    REQUIRE(contains(student, "below the attendance threshold"));

    // Stored in the services too.
    REQUIRE(w.grades.view_student_summary(w.session("admin01", "Admin1234"),
                                          "SRN-001").value()->cgpa == 9.0);
}

TEST_CASE("Invalid grade and attendance input is rejected through the CLI",
          "[cli][grades][attendance]") {
    World w;
    w.seed();

    const auto output = w.run({
        "1", "faculty01", "Teach1234",
        "2", "1", "SRN-001", "CS201", "3", "150",     // marks out of range
        "1", "SRN-001", "CS201", "3", "abc",          // not a number
        "2", "SRN-001", "CS201", "3", "Z",            // bad letter
        "1", "SRN-001", "CS999", "3", "80", "0",      // unknown course
        "3", "1", "SRN-001", "CS201", "3", "2026-02-30", "L1", "P",  // bad date
        "1", "SRN-001", "CS201", "3", "2026-11-01", "L1", "X",       // bad status
        "0", "0", "0"});

    REQUIRE(contains(output, "Invalid input"));
    REQUIRE(contains(output, "Invalid number"));
    REQUIRE(contains(output, "Unknown status"));
    REQUIRE_FALSE(contains(output, "Marks saved."));
    REQUIRE_FALSE(contains(output, "Attendance saved."));

    const auto admin = w.session("admin01", "Admin1234");
    REQUIRE(w.grades.view_student_grades(admin, "SRN-001").value()->empty());
}

TEST_CASE("The student report shows profile, grades, GPA and attendance",
          "[cli][reports]") {
    World w;
    w.seed();
    const auto faculty = w.session("faculty01", "Teach1234");
    REQUIRE(w.grades.enter_marks(faculty, "SRN-001", "CS201", 3, 95));
    REQUIRE(w.attendance.mark_attendance(
        faculty, "SRN-001", "CS201", 3, "2026-11-01", "Lecture 1",
        srs::attendance::AttendanceStatus::Present));

    // Student: Reports is module 4, "My academic report" is item 1.
    const auto output = w.run({
        "1", "student01", "Pass1234", "4", "1", "0", "0"});

    REQUIRE(contains(output, "Academic report"));
    REQUIRE(contains(output, "Alice Smith"));
    REQUIRE(contains(output, "SRN-001"));
    REQUIRE(contains(output, "Data Structures"));
    REQUIRE(contains(output, "CGPA: 10.00 (4 credits)"));
    REQUIRE(contains(output, "1 of 1 sessions present, 100.00%"));
}

TEST_CASE("Admin can view a student's report", "[cli][reports]") {
    World w;
    w.seed();
    const auto faculty = w.session("faculty01", "Teach1234");
    REQUIRE(w.grades.enter_letter_grade(faculty, "SRN-002", "CS201", 3, "B"));

    // Admin: Reports is module 5. "My academic report" is hidden for admins,
    // so "A student's academic report" is item 1.
    const auto output = w.run({
        "1", "admin01", "Admin1234", "5", "1", "SRN-002", "0", "0"});

    REQUIRE(contains(output, "Bob Jones"));
    REQUIRE(contains(output, "CGPA: 8.00 (4 credits)"));

    const auto unknown = w.run({
        "1", "admin01", "Admin1234", "5", "1", "SRN-404", "0", "0"});
    REQUIRE(contains(unknown, "No student found"));
}

TEST_CASE("Students cannot reach another student's data through the CLI",
          "[cli][rbac]") {
    World w;
    w.seed();
    const auto faculty = w.session("faculty01", "Teach1234");
    REQUIRE(w.grades.enter_marks(faculty, "SRN-002", "CS201", 3, 99));

    const auto output = w.run({
        "1", "student01", "Pass1234", "2", "1", "2", "0", "0", "0"});

    REQUIRE_FALSE(contains(output, "99.0"));
    REQUIRE_FALSE(contains(output, "Bob Jones"));
    REQUIRE(contains(output, "No grades recorded."));
}

// --- accounts --------------------------------------------------------------

TEST_CASE("Users can change their own password through the CLI",
          "[cli][account]") {
    World w;

    // Student: My account is module 5, "Change my password" is item 1.
    const auto output = w.run({
        "1", "student01", "Pass1234",
        "5", "1", "Pass1234", "NewPass5678", "NewPass5678", "0",
        "0"});

    REQUIRE(contains(output, "Password changed."));
    REQUIRE(w.auth.login("student01", "NewPass5678") != nullptr);
    REQUIRE(w.auth.login("student01", "Pass1234") == nullptr);
}

TEST_CASE("Mismatched or wrong passwords do not change anything",
          "[cli][account]") {
    World w;

    const auto output = w.run({
        "1", "student01", "Pass1234",
        "5", "1", "Pass1234", "NewPass5678", "Different999",  // mismatch
        "1", "WrongCurrent1", "NewPass5678", "NewPass5678",   // wrong current
        "0", "0"});

    REQUIRE(contains(output, "Passwords do not match"));
    REQUIRE_FALSE(contains(output, "Password changed."));
    REQUIRE(w.auth.login("student01", "Pass1234") != nullptr);
}

TEST_CASE("Admin can deactivate a user and that user can no longer log in",
          "[cli][account]") {
    World w;

    w.run({
        "1", "admin01", "Admin1234",
        "6", "3", "student02", "D", "YES", "0",
        "0", "0"});

    REQUIRE(w.auth.login("student02", "Pass5678") == nullptr);
}

// --- first-run setup -------------------------------------------------------

TEST_CASE("First-run setup collects a valid administrator", "[cli][setup]") {
    std::istringstream in("root\nStrongPass123\nStrongPass123\n");
    std::ostringstream out;

    const auto admin = srs::ui::prompt_first_admin(in, out);
    REQUIRE(admin);
    REQUIRE(admin->username == "root");
    REQUIRE(admin->password == "StrongPass123");
    REQUIRE(admin->role == Role::Admin);
}

TEST_CASE("First-run setup rejects bad input and gives up after three tries",
          "[cli][setup]") {
    // Mismatch, too-short password, invalid username: three failures.
    std::istringstream in(
        "root\nStrongPass123\nOtherPass123\n"
        "root\nshort\nshort\n"
        "bad name!\nStrongPass123\nStrongPass123\n");
    std::ostringstream out;

    REQUIRE_FALSE(srs::ui::prompt_first_admin(in, out));
    REQUIRE(contains(out.str(), "Passwords do not match"));
    REQUIRE(contains(out.str(), "password rules"));
    REQUIRE(contains(out.str(), "not valid"));
    REQUIRE(contains(out.str(), "Too many invalid attempts"));

    // Input ending early is also a clean "no".
    std::istringstream cut("root\n");
    std::ostringstream cut_out;
    REQUIRE_FALSE(srs::ui::prompt_first_admin(cut, cut_out));
}
