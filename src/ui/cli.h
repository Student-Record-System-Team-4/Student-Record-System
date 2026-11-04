#ifndef STUDENT_RECORD_SYSTEM_UI_CLI_H
#define STUDENT_RECORD_SYSTEM_UI_CLI_H

#include <iosfwd>
#include <utility>
#include <string>
#include <vector>

#include "attendance/attendance_service.h"
#include "auth/access_control.h"
#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "core/error_codes.h"
#include "course/course_service.h"
#include "grades/grade_service.h"
#include "student/student_profiles.h"
#include "reports/report_service.h"

namespace srs::ui {

struct NullOpt {};
constexpr NullOpt nullopt{};

template <typename T>
class Optional {
public:
    Optional() = default;
    Optional(NullOpt) {}
    Optional(const T& value) : value_(value), present_(true) {}
    Optional(T&& value) : value_(std::move(value)), present_(true) {}

    explicit operator bool() const { return present_; }
    T& operator*() { return value_; }
    const T& operator*() const { return value_; }
    T* operator->() { return &value_; }
    const T* operator->() const { return &value_; }

private:
    T value_{};
    bool present_ = false;
};

// Everything the CLI talks to. The CLI holds no business rules of its own: it
// prompts, calls these services and prints what they return. Which menu
// entries are shown is decided by asking UserAccountService::authorize() for
// each entry's permission, so the role -> permission matrix lives in one place.
struct Services {
    srs::auth::UserAccountService& accounts;
    srs::auth::AuthenticationService& authentication;
    srs::student::StudentProfileService& students;
    srs::course::CourseService& courses;
    srs::grades::GradeService& grades;
    srs::attendance::AttendanceService& attendance;
    srs::reports::ReportService& reports;
};

// First-run setup: asks for the initial administrator's credentials. Returns
// nullopt if input ends or the user gives up.
Optional<srs::auth::User> prompt_first_admin(
    std::istream& in, std::ostream& out);

class Cli {
public:
    Cli(Services services, std::istream& in, std::ostream& out);

    // Runs until the user exits or input ends. Returns 0 on a normal exit.
    int run();

private:
    using Action = void (Cli::*)();

    struct Item {
        const char* label;
        Optional<srs::auth::Permission> required;  // none = any user
        Action action;
    };

    struct Module {
        const char* name;
        std::vector<Item> items;
    };

    // --- screens ---------------------------------------------------------
    bool login_screen();
    void main_menu();
    void module_menu(const Module& module);
    void log_out();

    // --- prompts ---------------------------------------------------------
    Optional<std::string> text(const std::string& label);
    Optional<std::string> secret(const std::string& label);
    Optional<int> integer(const std::string& label, bool blank_is_zero = false);
    Optional<double> number(const std::string& label);
    Optional<std::string> text_or(const std::string& label, const std::string& current);
    Optional<int> integer_or(const std::string& label, int current);
    Optional<srs::auth::Role> role_choice(const std::string& label);
    bool confirm(const std::string& what);
    Optional<int> menu_choice(const std::string& prompt);

    // --- output ----------------------------------------------------------
    void fail(srs::core::ErrorCode error);
    void report(const srs::core::Result<void>& result, const std::string& success);
    bool allowed(srs::auth::Permission permission) const;
    std::string role_name() const;
    void print_profile(const srs::student::StudentProfile& profile);
    void print_grades(const std::vector<srs::grades::GradeRecord>& grades);
    void print_summary(const srs::grades::AcademicSummary& summary);
    void print_attendance_summary(const srs::attendance::AttendanceSummary& summary);
    void print_report(const srs::reports::StudentAcademicReport& report);
    Optional<srs::student::StudentProfile> find_student(const std::string& srn);

    // --- students --------------------------------------------------------
    void view_my_profile();
    void search_students();
    void register_student();
    void update_student();
    void deactivate_student();

    // --- courses and enrollment -------------------------------------------
    void list_courses();
    void create_course();
    void update_course();
    void deactivate_course();
    void enroll_student();
    void remove_student();
    void view_roster();

    // --- grades ----------------------------------------------------------
    void enter_marks();
    void enter_letter_grade();
    void view_my_grades();
    void view_my_summary();
    void view_student_grades();
    void view_student_summary();

    // --- attendance ------------------------------------------------------
    void mark_attendance();
    void view_my_attendance();
    void set_attendance_threshold();

    // --- reports ---------------------------------------------------------
    void my_report();
    void student_report();

    // --- users and account -----------------------------------------------
    void create_user();
    void change_user_role();
    void set_user_active();
    void reset_user_password();
    void change_my_password();

    Services services_;
    std::istream& in_;
    std::ostream& out_;
    std::vector<Module> modules_;
    std::string session_;   // empty = not logged in
    std::string username_;
    bool eof_ = false;
    bool quit_ = false;
};

}  // namespace srs::ui

#endif  // STUDENT_RECORD_SYSTEM_UI_CLI_H
