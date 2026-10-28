#ifndef STUDENT_RECORD_SYSTEM_AUTH_ACCESS_CONTROL_H
#define STUDENT_RECORD_SYSTEM_AUTH_ACCESS_CONTROL_H

namespace srs::auth {

enum class Role {
    Student,
    Faculty,
    Admin
};

// One entry per protected capability in the SRS. Other modules (courses,
// grades, attendance, reports) ask for the permission they need.
enum class Permission {
    ViewOwnProfile,
    ViewOwnGrades,
    ViewOwnAttendance,
    ViewOwnReports,
    EnterGrades,
    MarkAttendance,
    ViewCourseRoster,
    ViewCourseSummary,
    SearchStudents,
    ManageStudents,
    ManageCourses,
    EnrollStudents,
    ManageUsers,
    GenerateInstitutionReports
};

// SRS-F-003 / SRS-SR-002: the single role -> permission matrix.
class AccessControl {
public:
    static bool is_allowed(Role role, Permission permission);
};

}  // namespace srs::auth

#endif  // STUDENT_RECORD_SYSTEM_AUTH_ACCESS_CONTROL_H
