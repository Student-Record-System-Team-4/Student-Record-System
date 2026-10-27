#ifndef STUDENT_RECORD_SYSTEM_STUDENT_PROFILES_H
#define STUDENT_RECORD_SYSTEM_STUDENT_PROFILES_H

#include <string>
#include <vector>

#include "auth/authentication.h"
#include "core/result.h"
#include "core/validator.h"

namespace srs::student {

enum class UserRole {
    Student,
    Faculty,
    Admin
};

struct StudentProfile {
    std::string username;
    std::string name;
    std::string srn;
    std::string date_of_birth;
    std::string contact;
    std::string programme;
    int semester;
    bool active;
};

struct StudentSearchQuery {
    std::string name;
    std::string srn;
    std::string programme;
    int semester = 0;
};

class StudentProfileService {
public:
    explicit StudentProfileService(srs::auth::AuthenticationService& authentication);

    srs::core::Result<void> register_student(
        const StudentProfile& profile,
        UserRole requester);

    srs::core::Result<void> update_student(
        const std::string& srn,
        const StudentProfile& profile,
        UserRole requester);

    srs::core::Result<void> deactivate_student(
        const std::string& srn,
        UserRole requester);

    srs::core::Result<StudentProfile> view_own_profile(
        const std::string& username,
        UserRole requester) const;

    srs::core::Result<std::vector<StudentProfile>> search_students(
        const StudentSearchQuery& query,
        UserRole requester) const;

private:
    static bool valid_profile(const StudentProfile& profile);
    static bool contains_case_insensitive(
        const std::string& value,
        const std::string& query);

    srs::auth::AuthenticationService& authentication_;
    std::vector<StudentProfile> profiles_;
};

}  // namespace srs::student

#endif  // STUDENT_RECORD_SYSTEM_STUDENT_PROFILES_H
