
#ifndef STUDENT_RECORD_SYSTEM_REPORTS_REPORT_SERVICE_H
#define STUDENT_RECORD_SYSTEM_REPORTS_REPORT_SERVICE_H

#include <string>
#include <vector>

#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "core/result.h"
#include "course/course_service.h"
#include "grades/grade_service.h"
#include "attendance/attendance_service.h"
#include "student/student_profiles.h"

namespace srs::reports {

struct StudentAcademicReport {
    srs::student::StudentProfile profile;
    std::vector<srs::grades::GradeRecord> grades;
    srs::grades::AcademicSummary academic_summary;
    std::vector<srs::attendance::AttendanceSummary> attendance;
};

struct CoursePerformanceReport {
    std::string course_code;
    std::string course_title;
    int semester = 0;

    int enrolled_students = 0;
    int graded_students = 0;

    double average_grade_points = 0.0;
    double highest_grade_points = 0.0;
    double lowest_grade_points = 0.0;

    double average_attendance = 0.0;
};

class ReportService {
public:
    ReportService(
        srs::auth::UserAccountService& accounts,
        srs::auth::AuthenticationService& authentication,
        srs::student::StudentProfileService& students,
        srs::course::CourseService& courses,
        srs::grades::GradeService& grades,
        srs::attendance::AttendanceService& attendance);

    // Students can generate only their own report.
    srs::core::Result<StudentAcademicReport> generate_my_report(
        const std::string& session_id) const;

    // Admins can generate reports for any student.
    srs::core::Result<StudentAcademicReport> generate_student_report(
        const std::string& session_id,
        const std::string& srn) const;

    // Admin-only course performance report.
    // semester == 0 means all semesters.
    srs::core::Result<CoursePerformanceReport> generate_course_report(
        const std::string& session_id,
        const std::string& course_code,
        int semester = 0) const;

    // Export a student report to CSV.
    srs::core::Result<void> export_student_report_csv(
        const std::string& session_id,
        const std::string& srn,
        const std::string& file_path) const;

    // Export a student report to plain text.
    srs::core::Result<void> export_student_report_text(
        const std::string& session_id,
        const std::string& srn,
        const std::string& file_path) const;

private:
    srs::core::Result<StudentAcademicReport> build_student_report(
        const std::string& session_id,
        const std::string& srn) const;

    srs::auth::UserAccountService& accounts_;
    srs::auth::AuthenticationService& authentication_;
    srs::student::StudentProfileService& students_;
    srs::course::CourseService& courses_;
    srs::grades::GradeService& grades_;
    srs::attendance::AttendanceService& attendance_;
};

}  // namespace srs::reports

#endif  // STUDENT_RECORD_SYSTEM_REPORTS_REPORT_SERVICE_H
