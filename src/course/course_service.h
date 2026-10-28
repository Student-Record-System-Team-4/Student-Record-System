#ifndef STUDENT_RECORD_SYSTEM_COURSE_COURSE_SERVICE_H
#define STUDENT_RECORD_SYSTEM_COURSE_COURSE_SERVICE_H

#include <map>
#include <string>
#include <vector>

#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "core/result.h"
#include "student/student_profiles.h"

namespace srs::course {

struct Course {
    std::string code;
    std::string title;
    int credits = 0;
    std::string faculty_username;
};

struct RosterEntry {
    std::string srn;
    std::string name;
    int semester = 0;
};

// SRS-F-010 (courses), F-011 (enrolment), F-012 (rosters).
// Every call is identified by the caller's session id and is checked against
// the role -> permission matrix before it does anything.
class CourseService {
public:
    static constexpr int MIN_CREDITS = 1;
    static constexpr int MAX_CREDITS = 10;
    static constexpr int MIN_SEMESTER = 1;
    static constexpr int MAX_SEMESTER = 12;

    CourseService(
        srs::auth::UserAccountService& accounts,
        srs::auth::AuthenticationService& authentication,
        srs::student::StudentProfileService& students);

    // Admin only (ManageCourses).
    srs::core::Result<void> create_course(
        const std::string& session_id,
        const Course& course);

    // Admin only. The course code identifies the course and cannot change.
    srs::core::Result<void> update_course(
        const std::string& session_id,
        const Course& course);

    // Admin: every course. Faculty: only the courses assigned to them.
    srs::core::Result<std::vector<Course>> list_courses(
        const std::string& session_id) const;

    // Admin only (EnrollStudents). The student must be registered and active.
    srs::core::Result<void> enroll_student(
        const std::string& session_id,
        const std::string& srn,
        const std::string& course_code,
        int semester);

    // Admin: any course. Faculty: only their assigned courses.
    // semester == 0 returns every semester. Sorted by SRN.
    srs::core::Result<std::vector<RosterEntry>> view_roster(
        const std::string& session_id,
        const std::string& course_code,
        int semester = 0) const;

    // Queries for the grades / attendance modules (no session needed).
    // semester == 0 means "any semester".
    bool is_enrolled(
        const std::string& srn,
        const std::string& course_code,
        int semester = 0) const;

    bool is_assigned_faculty(
        const std::string& username,
        const std::string& course_code) const;

private:
    struct Enrollment {
        std::string srn;
        std::string course_code;
        int semester;
    };

    static bool valid_course(const Course& course);
    bool valid_faculty(const std::string& username) const;
    srs::core::Result<srs::student::StudentProfile> find_active_student(
        const std::string& srn) const;

    srs::auth::UserAccountService& accounts_;
    srs::auth::AuthenticationService& authentication_;
    srs::student::StudentProfileService& students_;
    std::map<std::string, Course> courses_;
    std::vector<Enrollment> enrollments_;
};

}  // namespace srs::course

#endif  // STUDENT_RECORD_SYSTEM_COURSE_COURSE_SERVICE_H
