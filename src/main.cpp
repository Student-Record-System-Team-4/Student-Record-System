#include <iostream>
#include <string>
#include <vector>

#include "attendance/attendance_service.h"
#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "course/course_service.h"
#include "grades/grade_service.h"
#include "persistence/database.h"
#include "persistence/repositories.h"
#include "reports/report_service.h"
#include "student/student_profiles.h"
#include "ui/cli.h"

int main(int argc, char** argv) {
    const std::string database_path =
        argc > 1 ? argv[1] : "student-record-system.db";

    srs::persistence::Database database(database_path);
    if (!database.is_open()) {
        std::cerr << "Unable to initialize database '" << database_path
                  << "': " << database.last_error() << '\n';
        return 1;
    }

    // A brand-new database has no users, so nobody could log in. Ask for the
    // first administrator and hand it to the authentication service, which
    // stores it like any other account.
    srs::persistence::UserRepository users(database);
    const auto stored = users.all();
    if (!stored) {
        std::cerr << "Unable to read user accounts from '" << database_path
                  << "'.\n";
        return 1;
    }

    std::vector<srs::auth::User> initial_users;
    if (stored.value()->empty()) {
        const auto admin = srs::ui::prompt_first_admin(std::cin, std::cout);
        if (!admin) {
            std::cerr << "Setup cancelled: no administrator was created.\n";
            return 1;
        }
        initial_users.push_back(*admin);
    }

    srs::auth::AuthenticationService authentication(initial_users, database);
    srs::auth::UserAccountService accounts(authentication);
    srs::student::StudentProfileService students(authentication, database);
    srs::course::CourseService courses(accounts, authentication, students, database);
    srs::grades::GradeService grades(accounts, authentication, students, courses, database);
    srs::attendance::AttendanceService attendance(
        accounts, authentication, students, courses, database);
    srs::reports::ReportService reports(
        accounts, authentication, students, courses, grades, attendance);

    srs::ui::Cli cli(
        {accounts, authentication, students, courses, grades, attendance, reports},
        std::cin, std::cout);
    return cli.run();
}
