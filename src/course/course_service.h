```cpp
#ifndef STUDENT_RECORD_SYSTEM_COURSE_COURSE_SERVICE_H
#define STUDENT_RECORD_SYSTEM_COURSE_COURSE_SERVICE_H

#include <map>
#include <string>
#include <vector>

#include "auth/authentication.h"
#include "auth/user_accounts.h"
#include "core/result.h"
#include "student/student_profiles.h"
#include "persistence/database.h"

namespace srs::course {

struct Course {
    std::string code;
    std::string title;
    int credits = 0;
    std::string faculty_username;
    bool active = true;
};

struct RosterEntry {
    std::string srn;
    std::string name;
    int semester = 0;
    std::string course_code;
};

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

    CourseService(
        srs::auth::UserAccountService& accounts,
        srs::auth::AuthenticationService& authentication,
        srs::student::StudentProfileService& students,
        srs::persistence::Database& database);

    srs::core::Result<void> create_course(
        const std::string& session_id,
        const Course& course);

    srs::core::Result<void> update_course(
        const std::string& session_id,
        const Course& course);

    srs::core::Result<std::vector<Course>> list_courses(
        const std::string& session_id) const;

    // Admin only (ManageCourses). Deactivates a course without deleting its
    // records; an inactive course rejects new enrollments.
    srs::core::Result<void> deactivate_course(
        const std::string& session_id,
        const std::string& course_code);

    // Admin only (EnrollStudents). The student must be registered and active.
    srs::core::Result<void> enroll_student(
        const std::string& session_id,
        const std::string& srn,
        const std::string& course_code,
        int semester);

    // Admin only (EnrollStudents). Removes a student's enrollment.
    srs::core::Result<void> remove_student(
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

    srs::core::Result<Course> get_course(
        const std::string& course_code) const;

    bool is_enrolled(
        const std::string& srn,
        const std::string& course_code,
        int semester = 0) const;

    bool is_assigned_faculty(
        const std::string& username,
        const std::string& course_code) const;

    // Students can view their own enrollments.
    // Admins can view enrollments for any student.
    srs::core::Result<std::vector<RosterEntry>> view_student_enrollments(
        const std::string& session_id,
        const std::string& srn) const;

private:
    struct Enrollment {
        std::string srn;
        std::string course_code;
        int semester = 0;
    };

    static bool valid_course(const Course& course);

    bool valid_faculty(const std::string& username) const;

    srs::core::Result<srs::student::StudentProfile> find_active_student(
        const std::string& session_id,
        const std::string& srn) const;

    srs::auth::UserAccountService& accounts_;
    srs::auth::AuthenticationService& authentication_;
    srs::student::StudentProfileService& students_;

    std::map<std::string, Course> courses_;
    std::vector<Enrollment> enrollments_;

    srs::persistence::Database* database_ = nullptr;
};

}  // namespace srs::course

#endif  // STUDENT_RECORD_SYSTEM_COURSE_COURSE_SERVICE_H
```