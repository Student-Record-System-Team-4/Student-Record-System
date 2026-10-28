#include "grades/grade_service.h"

#include <cctype>
#include <cmath>

namespace srs::grades {

using srs::auth::Permission;
using srs::core::ErrorCode;
using srs::core::Result;

namespace {

double round_two_decimals(double value) {
    return std::round(value * 100.0) / 100.0;
}

}  // namespace

std::string GradeScale::letter_for_marks(double marks) {
    if (marks >= 90.0) return "S";
    if (marks >= 80.0) return "A";
    if (marks >= 70.0) return "B";
    if (marks >= 60.0) return "C";
    if (marks >= 50.0) return "D";
    if (marks >= 40.0) return "E";
    return "F";
}

std::string GradeScale::normalize_letter(const std::string& letter) {
    std::string result;
    for (char ch : letter) {
        result.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(ch))));
    }
    return result;
}

Result<double> GradeScale::points_for_letter(const std::string& letter) {
    const std::string value = normalize_letter(letter);
    if (value == "S") return Result<double>::success(10.0);
    if (value == "A") return Result<double>::success(9.0);
    if (value == "B") return Result<double>::success(8.0);
    if (value == "C") return Result<double>::success(7.0);
    if (value == "D") return Result<double>::success(6.0);
    if (value == "E") return Result<double>::success(5.0);
    if (value == "F") return Result<double>::success(0.0);
    return Result<double>::failure(ErrorCode::INVALID_INPUT);
}

GradeService::GradeService(
    srs::auth::UserAccountService& accounts,
    srs::auth::AuthenticationService& authentication,
    srs::student::StudentProfileService& students,
    srs::course::CourseService& courses)
    : accounts_(accounts),
      authentication_(authentication),
      students_(students),
      courses_(courses) {}

Result<void> GradeService::check_entry(
    const std::string& session_id,
    const std::string& srn,
    const std::string& course_code,
    int semester) const {
    const auto allowed = accounts_.authorize(session_id, Permission::EnterGrades);
    if (!allowed) {
        return Result<void>::failure(allowed.error());
    }
    if (semester < srs::course::CourseService::MIN_SEMESTER ||
        semester > srs::course::CourseService::MAX_SEMESTER) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }
    const auto username = authentication_.username_for_session(session_id);
    if (!username) {
        return Result<void>::failure(ErrorCode::SESSION_ERROR);
    }
    // SRS-F-016: only the faculty assigned to this course may grade it.
    if (!courses_.is_assigned_faculty(*username.value(), course_code)) {
        return Result<void>::failure(ErrorCode::PERMISSION_DENIED);
    }
    // SRS-F-013: the student must be enrolled in that course and semester.
    if (!courses_.is_enrolled(srn, course_code, semester)) {
        return Result<void>::failure(ErrorCode::NOT_FOUND);
    }
    return Result<void>::success();
}

Result<void> GradeService::enter_marks(
    const std::string& session_id,
    const std::string& srn,
    const std::string& course_code,
    int semester,
    double marks) {
    const auto allowed = check_entry(session_id, srn, course_code, semester);
    if (!allowed) {
        return allowed;
    }
    if (std::isnan(marks) || marks < 0.0 || marks > 100.0) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }
    const std::string letter = GradeScale::letter_for_marks(marks);
    const auto points = GradeScale::points_for_letter(letter);
    grades_[Key(srn, course_code, semester)] =
        StoredGrade{true, marks, letter, *points.value()};
    return Result<void>::success();
}

Result<void> GradeService::enter_letter_grade(
    const std::string& session_id,
    const std::string& srn,
    const std::string& course_code,
    int semester,
    const std::string& letter) {
    const auto allowed = check_entry(session_id, srn, course_code, semester);
    if (!allowed) {
        return allowed;
    }
    const auto points = GradeScale::points_for_letter(letter);
    if (!points) {
        return Result<void>::failure(points.error());
    }
    grades_[Key(srn, course_code, semester)] =
        StoredGrade{false, 0.0, GradeScale::normalize_letter(letter), *points.value()};
    return Result<void>::success();
}

std::vector<GradeRecord> GradeService::records_for(const std::string& srn) const {
    std::vector<GradeRecord> records;
    for (const auto& entry : grades_) {
        if (std::get<0>(entry.first) != srn) {
            continue;
        }
        const auto course = courses_.get_course(std::get<1>(entry.first));
        if (!course) {
            continue;
        }
        GradeRecord record;
        record.srn = srn;
        record.course_code = course.value()->code;
        record.course_title = course.value()->title;
        record.semester = std::get<2>(entry.first);
        record.credits = course.value()->credits;
        record.has_marks = entry.second.has_marks;
        record.marks = entry.second.marks;
        record.letter = entry.second.letter;
        record.grade_points = entry.second.points;
        records.push_back(record);
    }
    return records;  // map order: by course code, then semester
}

AcademicSummary GradeService::compute_summary(const std::string& srn) const {
    struct Totals {
        double weighted_points = 0.0;
        int credits = 0;
    };
    std::map<int, Totals> per_semester;
    Totals overall;

    for (const auto& record : records_for(srn)) {
        const double weighted = record.grade_points * record.credits;
        per_semester[record.semester].weighted_points += weighted;
        per_semester[record.semester].credits += record.credits;
        overall.weighted_points += weighted;
        overall.credits += record.credits;
    }

    AcademicSummary summary;
    for (const auto& entry : per_semester) {
        SemesterGpa gpa;
        gpa.semester = entry.first;
        gpa.credits = entry.second.credits;
        gpa.gpa = round_two_decimals(entry.second.weighted_points / entry.second.credits);
        summary.semesters.push_back(gpa);
    }
    summary.total_credits = overall.credits;
    summary.cgpa = overall.credits > 0
                       ? round_two_decimals(overall.weighted_points / overall.credits)
                       : 0.0;
    return summary;
}

Result<std::string> GradeService::own_srn(const std::string& session_id) const {
    const auto allowed = accounts_.authorize(session_id, Permission::ViewOwnGrades);
    if (!allowed) {
        return Result<std::string>::failure(allowed.error());
    }
    const auto username = authentication_.username_for_session(session_id);
    if (!username) {
        return Result<std::string>::failure(ErrorCode::SESSION_ERROR);
    }
    const auto profile =
        students_.view_own_profile(*username.value(), srs::student::UserRole::Student);
    if (!profile) {
        return Result<std::string>::failure(ErrorCode::NOT_FOUND);
    }
    return Result<std::string>::success(profile.value()->srn);
}

Result<std::vector<GradeRecord>> GradeService::view_my_grades(
    const std::string& session_id) const {
    const auto srn = own_srn(session_id);
    if (!srn) {
        return Result<std::vector<GradeRecord>>::failure(srn.error());
    }
    return Result<std::vector<GradeRecord>>::success(records_for(*srn.value()));
}

Result<AcademicSummary> GradeService::view_my_summary(
    const std::string& session_id) const {
    const auto srn = own_srn(session_id);
    if (!srn) {
        return Result<AcademicSummary>::failure(srn.error());
    }
    return Result<AcademicSummary>::success(compute_summary(*srn.value()));
}

Result<std::vector<GradeRecord>> GradeService::view_student_grades(
    const std::string& session_id,
    const std::string& srn) const {
    const auto allowed = accounts_.authorize(session_id, Permission::ViewStudentGrades);
    if (!allowed) {
        return Result<std::vector<GradeRecord>>::failure(allowed.error());
    }
    return Result<std::vector<GradeRecord>>::success(records_for(srn));
}

Result<AcademicSummary> GradeService::view_student_summary(
    const std::string& session_id,
    const std::string& srn) const {
    const auto allowed = accounts_.authorize(session_id, Permission::ViewStudentGrades);
    if (!allowed) {
        return Result<AcademicSummary>::failure(allowed.error());
    }
    return Result<AcademicSummary>::success(compute_summary(srn));
}

}  // namespace srs::grades
