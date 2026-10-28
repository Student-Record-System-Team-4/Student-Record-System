#include "auth/access_control.h"

namespace srs::auth {

bool AccessControl::is_allowed(Role role, Permission permission) {
    switch (role) {
        case Role::Student:
            return permission == Permission::ViewOwnProfile ||
                   permission == Permission::ViewOwnGrades ||
                   permission == Permission::ViewOwnAttendance ||
                   permission == Permission::ViewOwnReports;

        case Role::Faculty:
            return permission == Permission::EnterGrades ||
                   permission == Permission::MarkAttendance ||
                   permission == Permission::ViewCourseRoster ||
                   permission == Permission::ViewCourseSummary;

        case Role::Admin:
            // Admins do not enter grades or attendance (SRS-F-016).
            return permission == Permission::SearchStudents ||
                   permission == Permission::ManageStudents ||
                   permission == Permission::ManageCourses ||
                   permission == Permission::EnrollStudents ||
                   permission == Permission::ManageUsers ||
                   permission == Permission::ViewStudentGrades ||
                   permission == Permission::ViewCourseRoster ||
                   permission == Permission::ViewCourseSummary ||
                   permission == Permission::GenerateInstitutionReports;
    }
    return false;
}

}  // namespace srs::auth
