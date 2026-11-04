
#ifndef STUDENT_RECORD_SYSTEM_ATTENDANCE_ATTENDANCE_SERVICE_H
#define STUDENT_RECORD_SYSTEM_ATTENDANCE_ATTENDANCE_SERVICE_H

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "core/result.h"
#include "course/course_service.h"
#include "student/student_profiles.h"

namespace srs::attendance {

enum class AttendanceStatus {
    Present,
    Absent,
    Late
};

struct AttendanceRecord {
    std::string srn;
    std::string course_code;
    int semester = 0;
    std::string date;
    std::string session_details;
    AttendanceStatus status = AttendanceStatus::Absent;
};

struct AttendanceSummary {
    std::string srn;
    std::string course_code;
    int semester = 0;
    int total_sessions = 0;
    int present_sessions = 0;
    double percentage = 0.0;
    bool below_threshold = false;
};

class AttendanceService {
public:
    static constexpr double DEFAULT_THRESHOLD = 75.0;

    AttendanceService(
        srs::auth::UserAccountService& accounts,
        srs::auth::AuthenticationService& authentication,
        srs::student::StudentProfileService& students,
        srs::course::CourseService& courses,
        double threshold = DEFAULT_THRESHOLD);

    // Configure persistent attendance storage.
    srs::core::Result<void> set_storage_file(
        const std::string& file_path);

    // F-017: Faculty records attendance.
    srs::core::Result<void> mark_attendance(
        const std::string& session_id,
        const std::string& srn,
        const std::string& course_code,
        int semester,
        const std::string& date,
        const std::string& session_details,
        AttendanceStatus status);

    // F-020: Student views their own attendance.
    srs::core::Result<std::vector<AttendanceRecord>>
    view_my_attendance(
        const std::string& session_id,
        const std::string& course_code) const;

    // F-018 and F-019: Student views their own percentage and threshold flag.
    srs::core::Result<AttendanceSummary> view_my_summary(
        const std::string& session_id,
        const std::string& course_code,
        int semester) const;

    // F-019: Admin configures the threshold.
    srs::core::Result<void> set_threshold(
        const std::string& session_id,
        double threshold);

private:
    using Key = std::tuple<
        std::string,
        std::string,
        int,
        std::string,
        std::string>;

    // Persistent storage operations.
    srs::core::Result<void> load_records();
    srs::core::Result<void> save_records() const;

    srs::core::Result<void> check_faculty_access(
        const std::string& session_id,
        const std::string& srn,
        const std::string& course_code,
        int semester) const;

    srs::core::Result<std::string> own_srn(
        const std::string& session_id) const;

    srs::core::Result<AttendanceSummary> calculate_percentage_for(
        const std::string& srn,
        const std::string& course_code,
        int semester) const;

    srs::auth::UserAccountService& accounts_;
    srs::auth::AuthenticationService& authentication_;
    srs::student::StudentProfileService& students_;
    srs::course::CourseService& courses_;

    double threshold_;
    std::map<Key, AttendanceRecord> records_;

    // Empty until persistent storage is configured.
    std::string storage_file_;
};

}  // namespace srs::attendance

#endif
