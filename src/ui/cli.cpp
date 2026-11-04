#include "ui/cli.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdio.h>
#include <utility>

#include "core/application.h"
#include "core/validator.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace srs::ui {

using srs::auth::Permission;
using srs::auth::Role;
using srs::core::ErrorCode;

namespace {

// ---------------------------------------------------------------------------
// Terminal helpers (input only: no business rules here)
// ---------------------------------------------------------------------------

bool stdin_is_terminal() {
#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    return handle != INVALID_HANDLE_VALUE &&
           GetConsoleMode(handle, &mode) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

// Turns terminal echo off while it is alive, so passwords are not displayed.
class EchoOff {
public:
    EchoOff() {
#ifdef _WIN32
        handle_ = GetStdHandle(STD_INPUT_HANDLE);
        if (handle_ != INVALID_HANDLE_VALUE && GetConsoleMode(handle_, &mode_)) {
            active_ = SetConsoleMode(handle_, mode_ & ~ENABLE_ECHO_INPUT) != 0;
        }
#else
        if (tcgetattr(STDIN_FILENO, &saved_) == 0) {
            termios silent = saved_;
            silent.c_lflag &= ~static_cast<tcflag_t>(ECHO);
            active_ = tcsetattr(STDIN_FILENO, TCSANOW, &silent) == 0;
        }
#endif
    }

    ~EchoOff() {
        if (!active_) {
            return;
        }
#ifdef _WIN32
        SetConsoleMode(handle_, mode_);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
#endif
    }

    EchoOff(const EchoOff&) = delete;
    EchoOff& operator=(const EchoOff&) = delete;

private:
    bool active_ = false;
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    DWORD mode_ = 0;
#else
    termios saved_{};
#endif
};

void strip_carriage_return(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

bool read_secret_line(std::istream& in, std::ostream& out, std::string& line) {
    if (&in == &std::cin && stdin_is_terminal()) {
        EchoOff guard;
        const bool ok = static_cast<bool>(std::getline(in, line));
        out << '\n';
        strip_carriage_return(line);
        return ok;
    }
    const bool ok = static_cast<bool>(std::getline(in, line));
    strip_carriage_return(line);
    return ok;
}

std::string trim(const std::string& value) {
    std::size_t first = 0;
    std::size_t last = value.size();
    while (first < last &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

std::string lower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

// Syntax only: "is this text a whole number / a finite number?". Whether the
// value is acceptable (range, semester, marks...) is the services' decision.
Optional<int> parse_int(const std::string& text) {
    if (text.empty() || text.size() > 10) {
        return nullopt;
    }
    std::size_t i = (text[0] == '-' || text[0] == '+') ? 1 : 0;
    if (i == text.size()) {
        return nullopt;
    }
    for (std::size_t j = i; j < text.size(); ++j) {
        if (!std::isdigit(static_cast<unsigned char>(text[j]))) {
            return nullopt;
        }
    }
    const long long value = std::strtoll(text.c_str(), nullptr, 10);
    if (value < -2000000000LL || value > 2000000000LL) {
        return nullopt;
    }
    return static_cast<int>(value);
}

Optional<double> parse_double(const std::string& text) {
    if (text.empty()) {
        return nullopt;
    }
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size() || !std::isfinite(value)) {
        return nullopt;
    }
    return value;
}

const char* error_message(ErrorCode error) {
    switch (error) {
        case ErrorCode::NONE:
            return "OK.";
        case ErrorCode::INVALID_INPUT:
            return "Invalid input. Please check the values and try again.";
        case ErrorCode::NOT_FOUND:
            return "Not found. Check the SRN, course code, semester or username.";
        case ErrorCode::ALREADY_EXISTS:
            return "That already exists.";
        case ErrorCode::PERMISSION_DENIED:
            return "You are not allowed to do that.";
        case ErrorCode::INVALID_CREDENTIALS:
            return "Invalid credentials.";
        case ErrorCode::HASHING_ERROR:
            return "Could not process the password.";
        case ErrorCode::SESSION_ERROR:
            return "Your session has ended. Please log in again.";
        case ErrorCode::INTERNAL_ERROR:
            return "Internal error. The change was not saved.";
    }
    return "Unknown error.";
}

const char* role_label(Role role) {
    switch (role) {
        case Role::Admin:
            return "Admin";
        case Role::Faculty:
            return "Faculty";
        case Role::Student:
            return "Student";
    }
    return "Unknown";
}

const char* attendance_label(srs::attendance::AttendanceStatus status) {
    switch (status) {
        case srs::attendance::AttendanceStatus::Present:
            return "Present";
        case srs::attendance::AttendanceStatus::Absent:
            return "Absent";
        case srs::attendance::AttendanceStatus::Late:
            return "Late";
    }
    return "?";
}

}  // namespace

// ---------------------------------------------------------------------------
// First-run setup
// ---------------------------------------------------------------------------

Optional<srs::auth::User> prompt_first_admin(
    std::istream& in, std::ostream& out) {
    out << "No user accounts exist yet. Create the first administrator.\n";

    for (int attempt = 0; attempt < 3; ++attempt) {
        std::string username;
        std::string password;
        std::string confirmation;

        out << "Admin username: " << std::flush;
        if (!std::getline(in, username)) {
            return nullopt;
        }
        username = trim(username);

        out << "Admin password: " << std::flush;
        if (!read_secret_line(in, out, password)) {
            return nullopt;
        }
        out << "Confirm password: " << std::flush;
        if (!read_secret_line(in, out, confirmation)) {
            return nullopt;
        }

        if (password != confirmation) {
            out << "Passwords do not match. Try again.\n";
            continue;
        }
        if (!srs::core::Validator::validate_username(username)) {
            out << "That username is not valid. Try again.\n";
            continue;
        }
        if (!srs::core::Validator::validate_password(password)) {
            out << "That password does not meet the password rules. "
                   "Try again.\n";
            continue;
        }
        return srs::auth::User{username, password, Role::Admin};
    }

    out << "Too many invalid attempts.\n";
    return nullopt;
}

// ---------------------------------------------------------------------------
// Construction: the menu table
// ---------------------------------------------------------------------------

Cli::Cli(Services services, std::istream& in, std::ostream& out)
    : services_(services), in_(in), out_(out) {
    modules_ = {
        {"Students",
         {{"View my profile", Permission::ViewOwnProfile, &Cli::view_my_profile},
          {"Search students", Permission::SearchStudents, &Cli::search_students},
          {"Register a student", Permission::ManageStudents, &Cli::register_student},
          {"Update a student", Permission::ManageStudents, &Cli::update_student},
          {"Deactivate a student", Permission::ManageStudents, &Cli::deactivate_student}}},
        {"Courses and enrollment",
         {{"List courses", Permission::ViewCourseRoster, &Cli::list_courses},
          {"Create a course", Permission::ManageCourses, &Cli::create_course},
          {"Update a course", Permission::ManageCourses, &Cli::update_course},
          {"Deactivate a course", Permission::ManageCourses, &Cli::deactivate_course},
          {"Enroll a student in a course", Permission::EnrollStudents, &Cli::enroll_student},
          {"Remove a student from a course", Permission::EnrollStudents, &Cli::remove_student},
          {"View a course roster", Permission::ViewCourseRoster, &Cli::view_roster}}},
        {"Grades",
         {{"Enter marks", Permission::EnterGrades, &Cli::enter_marks},
          {"Enter a letter grade", Permission::EnterGrades, &Cli::enter_letter_grade},
          {"View my grades", Permission::ViewOwnGrades, &Cli::view_my_grades},
          {"View my GPA and CGPA", Permission::ViewOwnGrades, &Cli::view_my_summary},
          {"View a student's grades", Permission::ViewStudentGrades, &Cli::view_student_grades},
          {"View a student's GPA and CGPA", Permission::ViewStudentGrades, &Cli::view_student_summary}}},
        {"Attendance",
         {{"Mark attendance", Permission::MarkAttendance, &Cli::mark_attendance},
          {"View my attendance", Permission::ViewOwnAttendance, &Cli::view_my_attendance},
          // set_threshold() is admin-only; ManageUsers is the admin-only
          // permission, so it decides who sees this entry.
          {"Set the low-attendance threshold", Permission::ManageUsers, &Cli::set_attendance_threshold}}},
        {"Reports",
         {{"My academic report", Permission::ViewOwnReports, &Cli::my_report},
          {"A student's academic report", Permission::ViewStudentGrades, &Cli::student_report}}},
        {"User management",
         {{"Create a user", Permission::ManageUsers, &Cli::create_user},
          {"Change a user's role", Permission::ManageUsers, &Cli::change_user_role},
          {"Activate or deactivate a user", Permission::ManageUsers, &Cli::set_user_active},
          {"Reset a user's password", Permission::ManageUsers, &Cli::reset_user_password}}},
        {"My account",
         {{"Change my password", nullopt, &Cli::change_my_password}}}};
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

int Cli::run() {
    out_ << student_record::application_name() << '\n';

    while (!eof_ && !quit_) {
        if (session_.empty()) {
            if (!login_screen()) {
                break;
            }
        } else {
            main_menu();
        }
    }

    if (!session_.empty()) {
        log_out();
    }
    out_ << "Goodbye.\n";
    return 0;
}

bool Cli::login_screen() {
    out_ << "\n=== Welcome ===\n"
         << "  1. Log in\n"
         << "  0. Exit\n";
    const auto choice = menu_choice("Choose an option");
    if (!choice) {
        return !eof_;
    }
    if (*choice == 0) {
        return false;
    }
    if (*choice != 1) {
        out_ << "Invalid choice. Please enter one of the listed options.\n";
        return true;
    }

    const auto username = text("Username");
    if (!username) {
        return !eof_;
    }
    const auto password = secret("Password");
    if (!password) {
        return !eof_;
    }

    const auto session = services_.authentication.login(*username, *password);
    if (!session) {
        out_ << "Login failed: wrong username or password, "
                "or the account is inactive.\n";
        return true;
    }

    session_ = session->id;
    username_ = session->username;
    out_ << "\nWelcome, " << username_ << " (" << role_name() << ").\n";
    return true;
}

void Cli::main_menu() {
    std::vector<const Module*> visible;
    for (const auto& module : modules_) {
        const bool any = std::any_of(
            module.items.begin(), module.items.end(), [this](const Item& item) {
                return !item.required || allowed(*item.required);
            });
        if (any) {
            visible.push_back(&module);
        }
    }

    out_ << "\n=== Main menu: " << username_ << " (" << role_name()
         << ") ===\n";
    for (std::size_t i = 0; i < visible.size(); ++i) {
        out_ << "  " << (i + 1) << ". " << visible[i]->name << '\n';
    }
    out_ << "  0. Log out\n"
         << "  q. Log out and exit\n";

    const auto line = text("Choose an option");
    if (!line) {
        return;
    }
    const std::string answer = lower(*line);
    if (answer == "q" || answer == "quit" || answer == "exit") {
        quit_ = true;
        return;
    }
    const auto choice = parse_int(answer);
    if (!choice) {
        out_ << "Invalid choice. Please enter one of the listed options.\n";
        return;
    }
    if (*choice == 0) {
        log_out();
        return;
    }
    if (*choice < 1 || static_cast<std::size_t>(*choice) > visible.size()) {
        out_ << "Invalid choice. Please enter one of the listed options.\n";
        return;
    }
    module_menu(*visible[static_cast<std::size_t>(*choice) - 1]);
}

void Cli::module_menu(const Module& module) {
    while (!eof_ && !quit_ && !session_.empty()) {
        std::vector<const Item*> visible;
        for (const auto& item : module.items) {
            if (!item.required || allowed(*item.required)) {
                visible.push_back(&item);
            }
        }
        if (visible.empty()) {
            return;
        }

        out_ << "\n--- " << module.name << " ---\n";
        for (std::size_t i = 0; i < visible.size(); ++i) {
            out_ << "  " << (i + 1) << ". " << visible[i]->label << '\n';
        }
        out_ << "  0. Back\n";

        const auto choice = menu_choice("Choose an option");
        if (!choice) {
            continue;
        }
        if (*choice == 0) {
            return;
        }
        if (*choice < 1 || static_cast<std::size_t>(*choice) > visible.size()) {
            out_ << "Invalid choice. Please enter one of the listed options.\n";
            continue;
        }
        (this->*(visible[static_cast<std::size_t>(*choice) - 1]->action))();
    }
}

void Cli::log_out() {
    services_.authentication.logout(session_);
    session_.clear();
    username_.clear();
    out_ << "You have been logged out.\n";
}

// ---------------------------------------------------------------------------
// Prompts
// ---------------------------------------------------------------------------

Optional<std::string> Cli::text(const std::string& label) {
    out_ << label << ": " << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        eof_ = true;
        out_ << '\n';
        return nullopt;
    }
    strip_carriage_return(line);
    return trim(line);
}

Optional<std::string> Cli::secret(const std::string& label) {
    out_ << label << ": " << std::flush;
    std::string line;
    if (!read_secret_line(in_, out_, line)) {
        eof_ = true;
        out_ << '\n';
        return nullopt;
    }
    return line;
}

Optional<int> Cli::integer(const std::string& label, bool blank_is_zero) {
    const auto line = text(label);
    if (!line) {
        return nullopt;
    }
    if (line->empty() && blank_is_zero) {
        return 0;
    }
    const auto value = parse_int(*line);
    if (!value) {
        out_ << "Invalid number. Cancelled.\n";
        return nullopt;
    }
    return value;
}

Optional<double> Cli::number(const std::string& label) {
    const auto line = text(label);
    if (!line) {
        return nullopt;
    }
    const auto value = parse_double(*line);
    if (!value) {
        out_ << "Invalid number. Cancelled.\n";
        return nullopt;
    }
    return value;
}

Optional<std::string> Cli::text_or(
    const std::string& label, const std::string& current) {
    const auto line = text(label + " [" + current + "]");
    if (!line) {
        return nullopt;
    }
    return line->empty() ? current : *line;
}

Optional<int> Cli::integer_or(const std::string& label, int current) {
    const auto line = text(label + " [" + std::to_string(current) + "]");
    if (!line) {
        return nullopt;
    }
    if (line->empty()) {
        return current;
    }
    const auto value = parse_int(*line);
    if (!value) {
        out_ << "Invalid number. Cancelled.\n";
        return nullopt;
    }
    return value;
}

Optional<Role> Cli::role_choice(const std::string& label) {
    const auto line = text(label + " (student/faculty/admin)");
    if (!line) {
        return nullopt;
    }
    const std::string answer = lower(*line);
    if (answer == "student") return Role::Student;
    if (answer == "faculty") return Role::Faculty;
    if (answer == "admin") return Role::Admin;
    out_ << "Unknown role. Cancelled.\n";
    return nullopt;
}

bool Cli::confirm(const std::string& what) {
    const auto line = text(what + " Type YES to confirm");
    if (line && *line == "YES") {
        return true;
    }
    if (line) {
        out_ << "Cancelled.\n";
    }
    return false;
}

Optional<int> Cli::menu_choice(const std::string& prompt) {
    const auto line = text(prompt);
    if (!line) {
        return nullopt;
    }
    const auto value = parse_int(*line);
    if (!value) {
        out_ << "Invalid choice. Please enter one of the listed options.\n";
        return nullopt;
    }
    return value;
}

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------

void Cli::fail(ErrorCode error) {
    out_ << error_message(error) << '\n';
    if (error == ErrorCode::SESSION_ERROR) {
        session_.clear();
        username_.clear();
    }
}

void Cli::report(const srs::core::Result<void>& result, const std::string& success) {
    if (result) {
        out_ << success << '\n';
    } else {
        fail(result.error());
    }
}

bool Cli::allowed(Permission permission) const {
    return static_cast<bool>(
        services_.accounts.authorize(session_, permission));
}

std::string Cli::role_name() const {
    const auto role = services_.authentication.role_of(username_);
    return role ? role_label(*role.value()) : "Unknown";
}

void Cli::print_profile(const srs::student::StudentProfile& profile) {
    out_ << "  Name:          " << profile.name << '\n'
         << "  SRN:           " << profile.srn << '\n'
         << "  Username:      " << profile.username << '\n'
         << "  Date of birth: " << profile.date_of_birth << '\n'
         << "  Contact:       " << profile.contact << '\n'
         << "  Programme:     " << profile.programme << '\n'
         << "  Semester:      " << profile.semester << '\n'
         << "  Status:        " << (profile.active ? "Active" : "Inactive") << '\n';
}

void Cli::print_grades(const std::vector<srs::grades::GradeRecord>& grades) {
    if (grades.empty()) {
        out_ << "  No grades recorded.\n";
        return;
    }
    auto sorted = grades;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const auto& a, const auto& b) {
                         return a.semester < b.semester;
                     });
    out_ << std::left << "  " << std::setw(5) << "Sem" << std::setw(9)
         << "Course" << std::setw(26) << "Title" << std::setw(4) << "Cr"
         << std::setw(8) << "Marks" << std::setw(7) << "Grade" << "Points\n";
    for (const auto& g : sorted) {
        std::ostringstream marks;
        if (g.has_marks) {
            marks << std::fixed << std::setprecision(1) << g.marks;
        } else {
            marks << "-";
        }
        out_ << "  " << std::setw(5) << g.semester << std::setw(9)
             << g.course_code << std::setw(26) << g.course_title
             << std::setw(4) << g.credits << std::setw(8) << marks.str()
             << std::setw(7) << g.letter << std::fixed
             << std::setprecision(1) << g.grade_points << '\n';
    }
    out_ << std::defaultfloat;
}

void Cli::print_summary(const srs::grades::AcademicSummary& summary) {
    if (summary.semesters.empty()) {
        out_ << "  No graded courses yet.\n";
        return;
    }
    for (const auto& semester : summary.semesters) {
        out_ << "  Semester " << semester.semester << ": GPA "
             << std::fixed << std::setprecision(2) << semester.gpa << " ("
             << semester.credits << " credits)\n";
    }
    out_ << "  CGPA: " << std::fixed << std::setprecision(2) << summary.cgpa
         << " (" << summary.total_credits << " credits)\n"
         << std::defaultfloat;
}

void Cli::print_attendance_summary(
    const srs::attendance::AttendanceSummary& summary) {
    out_ << "  " << summary.course_code << " (semester " << summary.semester
         << "): " << summary.present_sessions << " of "
         << summary.total_sessions << " sessions present, "
         << std::fixed << std::setprecision(2) << summary.percentage << "%"
         << std::defaultfloat
         << (summary.below_threshold ? "  ** below the attendance threshold **" : "")
         << '\n';
}

void Cli::print_report(
    const srs::reports::StudentAcademicReport& report) {
    out_ << "\n===== Academic report =====\n\nProfile\n";
    print_profile(report.profile);
    out_ << "\nGrades\n";
    print_grades(report.grades);
    out_ << "\nGPA and CGPA\n";
    print_summary(report.academic_summary);
    out_ << "\nAttendance\n";
    if (report.attendance.empty()) {
        out_ << "  No attendance records.\n";
    } else {
        for (const auto& summary : report.attendance) {
            print_attendance_summary(summary);
        }
    }
}

Optional<srs::student::StudentProfile> Cli::find_student(
    const std::string& srn) {
    srs::student::StudentSearchQuery query;
    query.srn = srn;
    const auto result = services_.students.search_students(session_, query);
    if (!result) {
        fail(result.error());
        return nullopt;
    }
    for (const auto& profile : *result.value()) {
        if (profile.srn == srn) {
            return profile;
        }
    }
    out_ << "No student found with that SRN.\n";
    return nullopt;
}

// ---------------------------------------------------------------------------
// Students
// ---------------------------------------------------------------------------

void Cli::view_my_profile() {
    const auto profile = services_.students.view_own_profile(session_);
    if (!profile) {
        fail(profile.error());
        return;
    }
    out_ << "\nMy profile\n";
    print_profile(*profile.value());
}

void Cli::search_students() {
    out_ << "Leave a field blank to ignore it.\n";
    srs::student::StudentSearchQuery query;
    const auto name = text("Name contains");
    if (!name) return;
    const auto srn = text("SRN contains");
    if (!srn) return;
    const auto programme = text("Programme contains");
    if (!programme) return;
    const auto semester = integer("Semester", true);
    if (!semester) return;
    query.name = *name;
    query.srn = *srn;
    query.programme = *programme;
    query.semester = *semester;

    const auto result = services_.students.search_students(session_, query);
    if (!result) {
        fail(result.error());
        return;
    }
    if (result.value()->empty()) {
        out_ << "No students match.\n";
        return;
    }
    out_ << std::left << "\n  " << std::setw(12) << "SRN" << std::setw(24)
         << "Name" << std::setw(20) << "Programme" << std::setw(5) << "Sem"
         << "Status\n";
    for (const auto& p : *result.value()) {
        out_ << "  " << std::setw(12) << p.srn << std::setw(24) << p.name
             << std::setw(20) << p.programme << std::setw(5) << p.semester
             << (p.active ? "Active" : "Inactive") << '\n';
    }
}

void Cli::register_student() {
    srs::student::StudentProfile profile;
    const auto username = text("Login username for the student");
    if (!username) return;
    const auto name = text("Full name");
    if (!name) return;
    const auto srn = text("SRN");
    if (!srn) return;
    const auto dob = text("Date of birth (YYYY-MM-DD)");
    if (!dob) return;
    const auto contact = text("Contact");
    if (!contact) return;
    const auto programme = text("Programme");
    if (!programme) return;
    const auto semester = integer("Semester");
    if (!semester) return;

    profile.username = *username;
    profile.name = *name;
    profile.srn = *srn;
    profile.date_of_birth = *dob;
    profile.contact = *contact;
    profile.programme = *programme;
    profile.semester = *semester;
    profile.active = true;
    report(services_.students.register_student(session_, profile),
           "Student registered.");
}

void Cli::update_student() {
    const auto srn = text("SRN of the student to update");
    if (!srn) return;
    auto current = find_student(*srn);
    if (!current) return;

    out_ << "Press Enter to keep the value in brackets.\n";
    const auto name = text_or("Full name", current->name);
    if (!name) return;
    const auto dob = text_or("Date of birth", current->date_of_birth);
    if (!dob) return;
    const auto contact = text_or("Contact", current->contact);
    if (!contact) return;
    const auto programme = text_or("Programme", current->programme);
    if (!programme) return;
    const auto semester = integer_or("Semester", current->semester);
    if (!semester) return;

    current->name = *name;
    current->date_of_birth = *dob;
    current->contact = *contact;
    current->programme = *programme;
    current->semester = *semester;
    report(services_.students.update_student(session_, *srn, *current),
           "Student updated.");
}

void Cli::deactivate_student() {
    const auto srn = text("SRN of the student to deactivate");
    if (!srn) return;
    if (!confirm("Deactivate student " + *srn + "?")) return;
    report(services_.students.deactivate_student(session_, *srn),
           "Student deactivated.");
}

// ---------------------------------------------------------------------------
// Courses and enrollment
// ---------------------------------------------------------------------------

void Cli::list_courses() {
    const auto result = services_.courses.list_courses(session_);
    if (!result) {
        fail(result.error());
        return;
    }
    if (result.value()->empty()) {
        out_ << "No courses.\n";
        return;
    }
    out_ << std::left << "\n  " << std::setw(10) << "Code" << std::setw(28)
         << "Title" << std::setw(9) << "Credits" << std::setw(16)
         << "Faculty" << "Status\n";
    for (const auto& c : *result.value()) {
        out_ << "  " << std::setw(10) << c.code << std::setw(28) << c.title
             << std::setw(9) << c.credits << std::setw(16)
             << c.faculty_username << (c.active ? "Active" : "Inactive")
             << '\n';
    }
}

void Cli::create_course() {
    const auto code = text("Course code");
    if (!code) return;
    const auto title = text("Title");
    if (!title) return;
    const auto credits = integer("Credits");
    if (!credits) return;
    const auto faculty = text("Faculty username");
    if (!faculty) return;
    report(services_.courses.create_course(
               session_,
               srs::course::Course{*code, *title, *credits, *faculty, true}),
           "Course created.");
}

void Cli::update_course() {
    const auto code = text("Code of the course to update");
    if (!code) return;
    const auto current = services_.courses.get_course(*code);
    if (!current) {
        fail(current.error());
        return;
    }
    out_ << "Press Enter to keep the value in brackets.\n";
    const auto title = text_or("Title", current.value()->title);
    if (!title) return;
    const auto credits = integer_or("Credits", current.value()->credits);
    if (!credits) return;
    const auto faculty = text_or("Faculty username", current.value()->faculty_username);
    if (!faculty) return;
    report(services_.courses.update_course(
               session_,
               srs::course::Course{*code, *title, *credits, *faculty,
                                   current.value()->active}),
           "Course updated.");
}

void Cli::deactivate_course() {
    const auto code = text("Code of the course to deactivate");
    if (!code) return;
    if (!confirm("Deactivate course " + *code + "?")) return;
    report(services_.courses.deactivate_course(session_, *code),
           "Course deactivated.");
}

void Cli::enroll_student() {
    const auto srn = text("Student SRN");
    if (!srn) return;
    const auto code = text("Course code");
    if (!code) return;
    const auto semester = integer("Semester");
    if (!semester) return;
    report(services_.courses.enroll_student(session_, *srn, *code, *semester),
           "Student enrolled.");
}

void Cli::remove_student() {
    const auto srn = text("Student SRN");
    if (!srn) return;
    const auto code = text("Course code");
    if (!code) return;
    const auto semester = integer("Semester");
    if (!semester) return;
    if (!confirm("Remove " + *srn + " from " + *code + "?")) return;
    report(services_.courses.remove_student(session_, *srn, *code, *semester),
           "Student removed from the course.");
}

void Cli::view_roster() {
    const auto code = text("Course code");
    if (!code) return;
    const auto semester = integer("Semester (blank = all)", true);
    if (!semester) return;
    const auto roster = services_.courses.view_roster(session_, *code, *semester);
    if (!roster) {
        fail(roster.error());
        return;
    }
    if (roster.value()->empty()) {
        out_ << "No students are enrolled.\n";
        return;
    }
    out_ << std::left << "\n  " << std::setw(12) << "SRN" << std::setw(26)
         << "Name" << "Semester\n";
    for (const auto& entry : *roster.value()) {
        out_ << "  " << std::setw(12) << entry.srn << std::setw(26)
             << entry.name << entry.semester << '\n';
    }
    out_ << "  " << roster.value()->size() << " enrolled.\n";
}

// ---------------------------------------------------------------------------
// Grades
// ---------------------------------------------------------------------------

void Cli::enter_marks() {
    const auto srn = text("Student SRN");
    if (!srn) return;
    const auto code = text("Course code");
    if (!code) return;
    const auto semester = integer("Semester");
    if (!semester) return;
    const auto marks = number("Marks (0-100)");
    if (!marks) return;
    report(services_.grades.enter_marks(session_, *srn, *code, *semester, *marks),
           "Marks saved.");
}

void Cli::enter_letter_grade() {
    const auto srn = text("Student SRN");
    if (!srn) return;
    const auto code = text("Course code");
    if (!code) return;
    const auto semester = integer("Semester");
    if (!semester) return;
    const auto letter = text("Letter grade (S/A/B/C/D/E/F)");
    if (!letter) return;
    report(services_.grades.enter_letter_grade(session_, *srn, *code, *semester, *letter),
           "Grade saved.");
}

void Cli::view_my_grades() {
    const auto grades = services_.grades.view_my_grades(session_);
    if (!grades) {
        fail(grades.error());
        return;
    }
    out_ << "\nMy grades\n";
    print_grades(*grades.value());
}

void Cli::view_my_summary() {
    const auto summary = services_.grades.view_my_summary(session_);
    if (!summary) {
        fail(summary.error());
        return;
    }
    out_ << "\nMy GPA and CGPA\n";
    print_summary(*summary.value());
}

void Cli::view_student_grades() {
    const auto srn = text("Student SRN");
    if (!srn) return;
    const auto grades = services_.grades.view_student_grades(session_, *srn);
    if (!grades) {
        fail(grades.error());
        return;
    }
    out_ << "\nGrades for " << *srn << '\n';
    print_grades(*grades.value());
}

void Cli::view_student_summary() {
    const auto srn = text("Student SRN");
    if (!srn) return;
    const auto summary = services_.grades.view_student_summary(session_, *srn);
    if (!summary) {
        fail(summary.error());
        return;
    }
    out_ << "\nGPA and CGPA for " << *srn << '\n';
    print_summary(*summary.value());
}

// ---------------------------------------------------------------------------
// Attendance
// ---------------------------------------------------------------------------

void Cli::mark_attendance() {
    const auto srn = text("Student SRN");
    if (!srn) return;
    const auto code = text("Course code");
    if (!code) return;
    const auto semester = integer("Semester");
    if (!semester) return;
    const auto date = text("Date (YYYY-MM-DD)");
    if (!date) return;
    const auto session = text("Session (e.g. Lecture 1)");
    if (!session) return;
    const auto status_text = text("Status (P = present, A = absent, L = late)");
    if (!status_text) return;

    const std::string answer = lower(*status_text);
    srs::attendance::AttendanceStatus status;
    if (answer == "p" || answer == "present") {
        status = srs::attendance::AttendanceStatus::Present;
    } else if (answer == "a" || answer == "absent") {
        status = srs::attendance::AttendanceStatus::Absent;
    } else if (answer == "l" || answer == "late") {
        status = srs::attendance::AttendanceStatus::Late;
    } else {
        out_ << "Unknown status. Cancelled.\n";
        return;
    }
    report(services_.attendance.mark_attendance(
               session_, *srn, *code, *semester, *date, *session, status),
           "Attendance saved. Marking the same session again corrects it.");
}

void Cli::view_my_attendance() {
    const auto code = text("Course code");
    if (!code) return;
    const auto records = services_.attendance.view_my_attendance(session_, *code);
    if (!records) {
        fail(records.error());
        return;
    }
    if (records.value()->empty()) {
        out_ << "No attendance has been recorded for this course yet.\n";
        return;
    }
    out_ << std::left << "\n  " << std::setw(5) << "Sem" << std::setw(13)
         << "Date" << std::setw(24) << "Session" << "Status\n";
    std::set<int> semesters;
    for (const auto& record : *records.value()) {
        semesters.insert(record.semester);
        out_ << "  " << std::setw(5) << record.semester << std::setw(13)
             << record.date << std::setw(24) << record.session_details
             << attendance_label(record.status) << '\n';
    }
    out_ << '\n';
    for (const int semester : semesters) {
        const auto summary =
            services_.attendance.view_my_summary(session_, *code, semester);
        if (summary) {
            print_attendance_summary(*summary.value());
        }
    }
}

void Cli::set_attendance_threshold() {
    const auto threshold = number("New low-attendance threshold (0-100)");
    if (!threshold) return;
    report(services_.attendance.set_threshold(session_, *threshold),
           "Threshold updated for this run. It is not saved between runs.");
}

// ---------------------------------------------------------------------------
// Reports (assembled only from what the services return)
// ---------------------------------------------------------------------------

void Cli::my_report() {
    const auto report = services_.reports.generate_my_report(session_);
    if (!report) {
        fail(report.error());
        return;
    }
    print_report(*report.value());
}

void Cli::student_report() {
    const auto srn = text("Student SRN");
    if (!srn) return;
    const auto report =
        services_.reports.generate_student_report(session_, *srn);
    if (!report) {
        if (report.error() == ErrorCode::NOT_FOUND) {
            out_ << "No student found with that SRN.\n";
            return;
        }
        fail(report.error());
        return;
    }
    print_report(*report.value());
}

// ---------------------------------------------------------------------------
// Users and account
// ---------------------------------------------------------------------------

void Cli::create_user() {
    const auto username = text("New username");
    if (!username) return;
    const auto password = secret("Password");
    if (!password) return;
    const auto again = secret("Confirm password");
    if (!again) return;
    if (*password != *again) {
        out_ << "Passwords do not match. Cancelled.\n";
        return;
    }
    const auto role = role_choice("Role");
    if (!role) return;
    report(services_.accounts.create_user(
               session_, srs::auth::User{*username, *password, *role}),
           "User created.");
}

void Cli::change_user_role() {
    const auto username = text("Username");
    if (!username) return;
    const auto role = role_choice("New role");
    if (!role) return;
    report(services_.accounts.set_user_role(session_, *username, *role),
           "Role updated.");
}

void Cli::set_user_active() {
    const auto username = text("Username");
    if (!username) return;
    const auto action = text("Activate or deactivate? (A/D)");
    if (!action) return;
    const std::string answer = lower(*action);
    if (answer == "a") {
        report(services_.accounts.set_user_active(session_, *username, true),
               "User activated.");
    } else if (answer == "d") {
        if (!confirm("Deactivate user " + *username + "?")) return;
        report(services_.accounts.set_user_active(session_, *username, false),
               "User deactivated.");
    } else {
        out_ << "Please answer A or D. Cancelled.\n";
    }
}

void Cli::reset_user_password() {
    const auto username = text("Username");
    if (!username) return;
    const auto password = secret("New password");
    if (!password) return;
    const auto again = secret("Confirm new password");
    if (!again) return;
    if (*password != *again) {
        out_ << "Passwords do not match. Cancelled.\n";
        return;
    }
    report(services_.accounts.reset_password(session_, *username, *password),
           "Password reset.");
}

void Cli::change_my_password() {
    const auto current = secret("Current password");
    if (!current) return;
    const auto password = secret("New password");
    if (!password) return;
    const auto again = secret("Confirm new password");
    if (!again) return;
    if (*password != *again) {
        out_ << "Passwords do not match. Cancelled.\n";
        return;
    }
    report(services_.accounts.change_password(session_, *current, *password),
           "Password changed.");
}

}  // namespace srs::ui
