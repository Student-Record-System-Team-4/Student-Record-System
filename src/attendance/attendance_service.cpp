#include "attendance/attendance_service.h"

#include <cctype>
#include <cmath>
#include <utility>
#include "persistence/repositories.h"

namespace srs::attendance {

using srs::auth::Permission;
using srs::auth::Role;
using srs::core::ErrorCode;
using srs::core::Result;

namespace {

bool has_non_whitespace(const std::string& value) {
    for (unsigned char ch : value) {
        if (!std::isspace(ch)) {
            return true;
        }
    }
    return false;
}

// Validate a real calendar date in YYYY-MM-DD format.
bool is_valid_date(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }

    for (std::size_t i = 0; i < date.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }

        if (date[i] < '0' || date[i] > '9') {
            return false;
        }
    }

    const int year =
        (date[0] - '0') * 1000 +
        (date[1] - '0') * 100 +
        (date[2] - '0') * 10 +
        (date[3] - '0');

    const int month =
        (date[5] - '0') * 10 + (date[6] - '0');

    const int day =
        (date[8] - '0') * 10 + (date[9] - '0');

    if (year < 1 || month < 1 || month > 12) {
        return false;
    }

    const bool leap_year =
        (year % 400 == 0) ||
        (year % 4 == 0 && year % 100 != 0);

    const int days_in_month[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    int max_days = days_in_month[month - 1];

    if (month == 2 && leap_year) {
        max_days = 29;
    }

    return day >= 1 && day <= max_days;
}

double round_two_decimals(double value) {
    return std::round(value * 100.0) / 100.0;
}

}  // namespace

AttendanceService::AttendanceService(
    srs::auth::UserAccountService& accounts,
    srs::auth::AuthenticationService& authentication,
    srs::student::StudentProfileService& students,
    srs::course::CourseService& courses,
    double threshold)
    : accounts_(accounts),
      authentication_(authentication),
      students_(students),
      courses_(courses),
      threshold_(
          std::isfinite(threshold) &&
          threshold >= 0.0 &&
          threshold <= 100.0
              ? threshold
              : DEFAULT_THRESHOLD) {}

AttendanceService::AttendanceService(
    srs::auth::UserAccountService& accounts,
    srs::auth::AuthenticationService& authentication,
    srs::student::StudentProfileService& students,
    srs::course::CourseService& courses,
    srs::persistence::Database& database,
    double threshold)
    : AttendanceService(accounts, authentication, students, courses, threshold) {
    database_ = &database;
    srs::persistence::AttendanceRepository repository(database);
    const auto rows = repository.all();
    if (rows) for (const auto& row : *rows.value()) {
        const auto status = row.status == "Present" ? AttendanceStatus::Present :
            row.status == "Late" ? AttendanceStatus::Late : AttendanceStatus::Absent;
        records_[Key(row.srn, row.course_code, row.semester, row.date,
                     row.session_details)] =
            {row.srn, row.course_code, row.semester, row.date,
             row.session_details, status};
    }
}

// Check faculty permission, course assignment and student enrolment.
Result<void> AttendanceService::check_faculty_access(
    const std::string& session_id,
    const std::string& srn,
    const std::string& course_code,
    int semester) const {

    const auto allowed =
        accounts_.authorize(session_id, Permission::MarkAttendance);

    if (!allowed) {
        return Result<void>::failure(allowed.error());
    }

    const auto username =
        authentication_.username_for_session(session_id);

    if (!username) {
        return Result<void>::failure(ErrorCode::SESSION_ERROR);
    }

    if (semester < srs::course::CourseService::MIN_SEMESTER ||
        semester > srs::course::CourseService::MAX_SEMESTER) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    const auto course = courses_.get_course(course_code);

    if (!course) {
        return Result<void>::failure(course.error());
    }

    if (!courses_.is_assigned_faculty(
            *username.value(), course_code)) {
        return Result<void>::failure(ErrorCode::PERMISSION_DENIED);
    }

    if (!courses_.is_enrolled(srn, course_code, semester)) {
        return Result<void>::failure(ErrorCode::NOT_FOUND);
    }

    return Result<void>::success();
}

// F-017: Record or correct attendance for one student and one session.
Result<void> AttendanceService::mark_attendance(
    const std::string& session_id,
    const std::string& srn,
    const std::string& course_code,
    int semester,
    const std::string& date,
    const std::string& session_details,
    AttendanceStatus status) {

    if (!has_non_whitespace(srn) ||
        !has_non_whitespace(course_code) ||
        !has_non_whitespace(session_details) ||
        !is_valid_date(date)) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    if (status != AttendanceStatus::Present &&
        status != AttendanceStatus::Absent &&
        status != AttendanceStatus::Late) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    const auto access = check_faculty_access(
        session_id, srn, course_code, semester);

    if (!access) {
        return Result<void>::failure(access.error());
    }

    const Key key(
        srn, course_code, semester, date, session_details);

    AttendanceRecord record{
        srn,
        course_code,
        semester,
        date,
        session_details,
        status
    };

    // If the same session is entered again, update the existing record.
    records_[key] = std::move(record);
    if (database_) {
        srs::persistence::AttendanceRepository repository(*database_);
        const char* status_name = status == AttendanceStatus::Present ? "Present" :
            status == AttendanceStatus::Late ? "Late" : "Absent";
        if (!repository.save({srn, course_code, date, session_details,
                              status_name, semester}))
            return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    return Result<void>::success();
}

// F-018 and F-019: Internal attendance calculation.
Result<AttendanceSummary>
AttendanceService::calculate_percentage_for(
    const std::string& srn,
    const std::string& course_code,
    int semester) const {

    if (!has_non_whitespace(srn) ||
        !has_non_whitespace(course_code) ||
        semester < srs::course::CourseService::MIN_SEMESTER ||
        semester > srs::course::CourseService::MAX_SEMESTER) {
        return Result<AttendanceSummary>::failure(
            ErrorCode::INVALID_INPUT);
    }

    if (!courses_.is_enrolled(srn, course_code, semester)) {
        return Result<AttendanceSummary>::failure(
            ErrorCode::NOT_FOUND);
    }

    AttendanceSummary summary;
    summary.srn = srn;
    summary.course_code = course_code;
    summary.semester = semester;

    for (const auto& entry : records_) {
        const AttendanceRecord& record = entry.second;

        if (record.srn == srn &&
            record.course_code == course_code &&
            record.semester == semester) {

            summary.total_sessions++;

            // Only Present counts in the numerator.
            // Absent and Late both count in total sessions.
            if (record.status == AttendanceStatus::Present) {
                summary.present_sessions++;
            }
        }
    }

    if (summary.total_sessions > 0) {
        summary.percentage = round_two_decimals(
            (static_cast<double>(summary.present_sessions) /
             summary.total_sessions) * 100.0);
    }

    summary.below_threshold =
        summary.percentage < threshold_;

    return Result<AttendanceSummary>::success(summary);
}

// Identify the SRN belonging to the logged-in student.
Result<std::string> AttendanceService::own_srn(
    const std::string& session_id) const {

    const auto allowed =
        accounts_.authorize(session_id, Permission::ViewOwnAttendance);

    if (!allowed) {
        return Result<std::string>::failure(allowed.error());
    }

    const auto username =
        authentication_.username_for_session(session_id);

    if (!username) {
        return Result<std::string>::failure(ErrorCode::SESSION_ERROR);
    }

    const auto profile = students_.view_own_profile(session_id);

    if (!profile) {
        return Result<std::string>::failure(ErrorCode::NOT_FOUND);
    }

    return Result<std::string>::success(profile.value()->srn);
}

// F-020: Return only the attendance belonging to the logged-in student.
Result<std::vector<AttendanceRecord>>
AttendanceService::view_my_attendance(
    const std::string& session_id,
    const std::string& course_code) const {

    if (!has_non_whitespace(course_code)) {
        return Result<std::vector<AttendanceRecord>>::failure(
            ErrorCode::INVALID_INPUT);
    }

    const auto srn = own_srn(session_id);

    if (!srn) {
        return Result<std::vector<AttendanceRecord>>::failure(
            srn.error());
    }

    const auto course = courses_.get_course(course_code);

    if (!course) {
        return Result<std::vector<AttendanceRecord>>::failure(
            course.error());
    }

    if (!courses_.is_enrolled(*srn.value(), course_code)) {
        return Result<std::vector<AttendanceRecord>>::failure(
            ErrorCode::NOT_FOUND);
    }

    std::vector<AttendanceRecord> result;

    for (const auto& entry : records_) {
        const AttendanceRecord& record = entry.second;

        if (record.srn == *srn.value() &&
            record.course_code == course_code) {
            result.push_back(record);
        }
    }

    return Result<std::vector<AttendanceRecord>>::success(
        std::move(result));
}

// F-020: Student views their own percentage and warning.
Result<AttendanceSummary> AttendanceService::view_my_summary(
    const std::string& session_id,
    const std::string& course_code,
    int semester) const {

    const auto srn = own_srn(session_id);

    if (!srn) {
        return Result<AttendanceSummary>::failure(srn.error());
    }

    return calculate_percentage_for(
        *srn.value(), course_code, semester);
}

// F-019: Only an authenticated admin can configure the threshold.
Result<void> AttendanceService::set_threshold(
    const std::string& session_id,
    double threshold) {

    const auto username =
        authentication_.username_for_session(session_id);

    if (!username) {
        return Result<void>::failure(ErrorCode::SESSION_ERROR);
    }

    const auto role =
        authentication_.role_of(*username.value());

    if (!role) {
        return Result<void>::failure(role.error());
    }

    if (*role.value() != Role::Admin) {
        return Result<void>::failure(
            ErrorCode::PERMISSION_DENIED);
    }

    if (!std::isfinite(threshold) ||
        threshold < 0.0 || threshold > 100.0) {
        return Result<void>::failure(
            ErrorCode::INVALID_INPUT);
    }

    threshold_ = threshold;

    return Result<void>::success();
}

}  // namespace srs::attendance