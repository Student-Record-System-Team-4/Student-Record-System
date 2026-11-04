
#include "reports/report_service.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace srs::reports {

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

std::string csv_escape(const std::string& value) {
    std::string result = "\"";

    for (char ch : value) {
        if (ch == '"') {
            result += "\"\"";
        } else {
            result += ch;
        }
    }

    result += "\"";
    return result;
}

}  // namespace

ReportService::ReportService(
    srs::auth::UserAccountService& accounts,
    srs::auth::AuthenticationService& authentication,
    srs::student::StudentProfileService& students,
    srs::course::CourseService& courses,
    srs::grades::GradeService& grades,
    srs::attendance::AttendanceService& attendance)
    : accounts_(accounts),
      authentication_(authentication),
      students_(students),
      courses_(courses),
      grades_(grades),
      attendance_(attendance) {}

// Builds a student's report after checking the user's role.
Result<StudentAcademicReport> ReportService::build_student_report(
    const std::string& session_id,
    const std::string& srn) const {

    if (!has_non_whitespace(srn)) {
        return Result<StudentAcademicReport>::failure(
            ErrorCode::INVALID_INPUT);
    }

    const auto username =
        authentication_.username_for_session(session_id);

    if (!username) {
        return Result<StudentAcademicReport>::failure(
            username.error());
    }

    const auto role =
        authentication_.role_of(*username.value());

    if (!role) {
        return Result<StudentAcademicReport>::failure(role.error());
    }

    StudentAcademicReport report;

    if (*role.value() == Role::Student) {
        const auto allowed =
            accounts_.authorize(session_id, Permission::ViewOwnReports);

        if (!allowed) {
            return Result<StudentAcademicReport>::failure(
                allowed.error());
        }

        const auto profile = students_.view_own_profile(session_id);

        if (!profile) {
            return Result<StudentAcademicReport>::failure(
                profile.error());
        }

        if (profile.value()->srn != srn) {
            return Result<StudentAcademicReport>::failure(
                ErrorCode::PERMISSION_DENIED);
        }

        report.profile = *profile.value();

        const auto student_grades =
            grades_.view_my_grades(session_id);

        if (!student_grades) {
            return Result<StudentAcademicReport>::failure(
                student_grades.error());
        }

        report.grades = *student_grades.value();

        const auto academic_summary =
            grades_.view_my_summary(session_id);

        if (!academic_summary) {
            return Result<StudentAcademicReport>::failure(
                academic_summary.error());
        }

        report.academic_summary = *academic_summary.value();
    }
    else if (*role.value() == Role::Admin) {
        const auto allowed =
            accounts_.authorize(
                session_id,
                Permission::GenerateInstitutionReports);

        if (!allowed) {
            return Result<StudentAcademicReport>::failure(
                allowed.error());
        }

        srs::student::StudentSearchQuery query;
        query.srn = srn;

        const auto matches = students_.search_students(
            session_id,
            query);

        if (!matches) {
            return Result<StudentAcademicReport>::failure(
                matches.error());
        }

        bool found = false;

        for (const auto& profile : *matches.value()) {
            if (profile.srn == srn) {
                report.profile = profile;
                found = true;
                break;
            }
        }

        if (!found) {
            return Result<StudentAcademicReport>::failure(
                ErrorCode::NOT_FOUND);
        }

        const auto student_grades =
            grades_.view_student_grades(session_id, srn);

        if (!student_grades) {
            return Result<StudentAcademicReport>::failure(
                student_grades.error());
        }

        report.grades = *student_grades.value();

        const auto academic_summary =
            grades_.view_student_summary(session_id, srn);

        if (!academic_summary) {
            return Result<StudentAcademicReport>::failure(
                academic_summary.error());
        }

        report.academic_summary = *academic_summary.value();
    }
    else {
        return Result<StudentAcademicReport>::failure(
            ErrorCode::PERMISSION_DENIED);
    }

    // Retrieve the student's enrolled courses.
    const auto enrollments =
        courses_.view_student_enrollments(session_id, srn);

    if (!enrollments) {
        return Result<StudentAcademicReport>::failure(
            enrollments.error());
    }

    // Retrieve attendance for every enrolled course.
    for (const auto& enrollment : *enrollments.value()) {
        Result<srs::attendance::AttendanceSummary> attendance_summary =
            (*role.value() == Role::Student)
                ? attendance_.view_my_summary(
                      session_id,
                      enrollment.course_code,
                      enrollment.semester)
                : attendance_.view_student_summary(
                      session_id,
                      srn,
                      enrollment.course_code,
                      enrollment.semester);

        if (!attendance_summary) {
            return Result<StudentAcademicReport>::failure(
                attendance_summary.error());
        }

        report.attendance.push_back(*attendance_summary.value());
    }

    return Result<StudentAcademicReport>::success(std::move(report));
}

// F-021: Generate the logged-in student's own report.
Result<StudentAcademicReport> ReportService::generate_my_report(
    const std::string& session_id) const {

    const auto username =
        authentication_.username_for_session(session_id);

    if (!username) {
        return Result<StudentAcademicReport>::failure(
            username.error());
    }

    const auto profile = students_.view_own_profile(session_id);

    if (!profile) {
        return Result<StudentAcademicReport>::failure(
            profile.error());
    }

    return build_student_report(session_id, profile.value()->srn);
}

// F-021: Admin can generate a report for any active student.
Result<StudentAcademicReport> ReportService::generate_student_report(
    const std::string& session_id,
    const std::string& srn) const {

    const auto allowed =
        accounts_.authorize(
            session_id,
            Permission::GenerateInstitutionReports);

    if (!allowed) {
        return Result<StudentAcademicReport>::failure(
            allowed.error());
    }

    return build_student_report(session_id, srn);
}

// F-023: Generate course-level performance statistics.
Result<CoursePerformanceReport> ReportService::generate_course_report(
    const std::string& session_id,
    const std::string& course_code,
    int semester) const {

    const auto allowed =
        accounts_.authorize(
            session_id,
            Permission::GenerateInstitutionReports);

    if (!allowed) {
        return Result<CoursePerformanceReport>::failure(
            allowed.error());
    }

    if (!has_non_whitespace(course_code) || semester < 0) {
        return Result<CoursePerformanceReport>::failure(
            ErrorCode::INVALID_INPUT);
    }

    const auto course = courses_.get_course(course_code);

    if (!course) {
        return Result<CoursePerformanceReport>::failure(
            course.error());
    }

    const auto roster =
        courses_.view_roster(session_id, course_code, semester);

    if (!roster) {
        return Result<CoursePerformanceReport>::failure(
            roster.error());
    }

    CoursePerformanceReport report;
    report.course_code = course.value()->code;
    report.course_title = course.value()->title;
    report.semester = semester;
    report.enrolled_students =
        static_cast<int>(roster.value()->size());

    double total_grade_points = 0.0;
    double highest = -1.0;
    double lowest = 11.0;
    double total_attendance = 0.0;

    for (const auto& student : *roster.value()) {
        const auto student_grades =
            grades_.view_student_grades(session_id, student.srn);

        if (!student_grades) {
            return Result<CoursePerformanceReport>::failure(
                student_grades.error());
        }

        for (const auto& grade : *student_grades.value()) {
            if (grade.course_code == course_code &&
                grade.semester == student.semester &&
                grade.has_marks) {

                const double points = grade.grade_points;

                total_grade_points += points;
                highest = std::max(highest, points);
                lowest = std::min(lowest, points);
                ++report.graded_students;
            }
        }

        const auto attendance_summary =
            attendance_.view_student_summary(
                session_id,
                student.srn,
                course_code,
                student.semester);

        if (!attendance_summary) {
            return Result<CoursePerformanceReport>::failure(
                attendance_summary.error());
        }

        total_attendance +=
            attendance_summary.value()->percentage;
    }

    if (report.graded_students > 0) {
        report.average_grade_points =
            total_grade_points / report.graded_students;

        report.highest_grade_points = highest;
        report.lowest_grade_points = lowest;
    }

    if (report.enrolled_students > 0) {
        report.average_attendance =
            total_attendance / report.enrolled_students;
    }

    return Result<CoursePerformanceReport>::success(
        std::move(report));
}

// F-022: Export a student report as CSV.
Result<void> ReportService::export_student_report_csv(
    const std::string& session_id,
    const std::string& srn,
    const std::string& file_path) const {

    if (!has_non_whitespace(file_path)) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    // Supports both students exporting their own report and admins
    // exporting another student's report.
    const auto report = build_student_report(session_id, srn);

    if (!report) {
        return Result<void>::failure(report.error());
    }

    std::ofstream file(file_path);

    if (!file.is_open()) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    file << "STUDENT ACADEMIC REPORT\n";
    file << "SRN,Name,Programme,Current Semester\n";

    file << csv_escape(report.value()->profile.srn) << ','
         << csv_escape(report.value()->profile.name) << ','
         << csv_escape(report.value()->profile.programme) << ','
         << report.value()->profile.semester << '\n';

    file << "\nSEMESTER-WISE GPA\n";
    file << "Semester,Credits,GPA\n";

    for (const auto& semester :
         report.value()->academic_summary.semesters) {

        file << semester.semester << ','
             << semester.credits << ','
             << std::fixed << std::setprecision(2)
             << semester.gpa << '\n';
    }

    file << "\nCGPA SUMMARY\n";
    file << "Total Credits,CGPA\n";

    file << report.value()->academic_summary.total_credits << ','
         << std::fixed << std::setprecision(2)
         << report.value()->academic_summary.cgpa << '\n';

    file << "\nCOURSE GRADES\n";
    file << "Course Code,Course Title,Semester,Credits,Marks,Letter Grade,Grade Points\n";

    for (const auto& grade : report.value()->grades) {
        file << csv_escape(grade.course_code) << ','
             << csv_escape(grade.course_title) << ','
             << grade.semester << ','
             << grade.credits << ',';

        if (grade.has_marks) {
            file << grade.marks;
        }

        file << ','
             << csv_escape(grade.letter) << ','
             << grade.grade_points << '\n';
    }

    file << "\nATTENDANCE SUMMARY\n";
    file << "Course Code,Semester,Total Sessions,Present Sessions,Percentage,Below Threshold\n";

    for (const auto& item : report.value()->attendance) {
        file << csv_escape(item.course_code) << ','
             << item.semester << ','
             << item.total_sessions << ','
             << item.present_sessions << ','
             << item.percentage << ','
             << (item.below_threshold ? "Yes" : "No")
             << '\n';
    }

    file.close();

    if (!file) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    return Result<void>::success();
}

// F-022: Export a student report as plain text.
Result<void> ReportService::export_student_report_text(
    const std::string& session_id,
    const std::string& srn,
    const std::string& file_path) const {

    if (!has_non_whitespace(file_path)) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    const auto report = build_student_report(session_id, srn);

    if (!report) {
        return Result<void>::failure(report.error());
    }

    std::ofstream file(file_path);

    if (!file.is_open()) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    file << "========================================\n";
    file << "         STUDENT ACADEMIC REPORT\n";
    file << "========================================\n\n";

    file << "STUDENT PROFILE\n";
    file << "SRN: " << report.value()->profile.srn << '\n';
    file << "Name: " << report.value()->profile.name << '\n';
    file << "Programme: " << report.value()->profile.programme << '\n';
    file << "Current Semester: "
         << report.value()->profile.semester << "\n\n";

    file << "SEMESTER-WISE GPA\n";

    for (const auto& semester :
         report.value()->academic_summary.semesters) {

        file << "Semester " << semester.semester
             << " | Credits: " << semester.credits
             << " | GPA: " << std::fixed << std::setprecision(2)
             << semester.gpa << '\n';
    }

    file << "\nTotal Credits: "
         << report.value()->academic_summary.total_credits << '\n';

    file << "CGPA: " << std::fixed << std::setprecision(2)
         << report.value()->academic_summary.cgpa << "\n\n";

    file << "COURSE GRADES\n";

    for (const auto& grade : report.value()->grades) {
        file << grade.course_code << " - "
             << grade.course_title
             << " | Semester: " << grade.semester
             << " | Credits: " << grade.credits
             << " | ";

        if (grade.has_marks) {
            file << "Marks: " << grade.marks << " | ";
        } else {
            file << "Marks: Not entered | ";
        }

        file << "Grade: " << grade.letter
             << " | Grade Points: " << grade.grade_points
             << '\n';
    }

    file << "\nATTENDANCE SUMMARY\n";

    for (const auto& item : report.value()->attendance) {
        file << item.course_code
             << " | Semester: " << item.semester
             << " | Sessions: " << item.total_sessions
             << " | Present: " << item.present_sessions
             << " | Attendance: " << std::fixed
             << std::setprecision(2) << item.percentage << "%"
             << " | Below Threshold: "
             << (item.below_threshold ? "Yes" : "No")
             << '\n';
    }

    file.close();

    if (!file) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    return Result<void>::success();
}

}  // namespace srs::reports
