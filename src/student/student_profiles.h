#ifndef STUDENT_RECORD_SYSTEM_STUDENT_PROFILES_H
#define STUDENT_RECORD_SYSTEM_STUDENT_PROFILES_H

#include <string>
#include <vector>

#include "auth/authentication.h"
#include "core/result.h"
#include "core/validator.h"
#include "persistence/database.h"

namespace srs::student {

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
    StudentProfileService(
        srs::auth::AuthenticationService& authentication,
        srs::persistence::Database& database);

    srs::core::Result<void> register_student(
        const std::string& session_id,
        const StudentProfile& profile);

    srs::core::Result<void> update_student(
        const std::string& session_id,
        const std::string& srn,
        const StudentProfile& profile);

    srs::core::Result<void> deactivate_student(
        const std::string& session_id,
        const std::string& srn);

    srs::core::Result<StudentProfile> view_own_profile(
        const std::string& session_id) const;

    srs::core::Result<std::vector<StudentProfile>> search_students(
        const std::string& session_id,
        const StudentSearchQuery& query) const;

private:
    static bool valid_profile(const StudentProfile& profile);
    static bool contains_case_insensitive(
        const std::string& value,
        const std::string& query);

    srs::auth::AuthenticationService& authentication_;
    std::vector<StudentProfile> profiles_;
    srs::persistence::Database* database_ = nullptr;
};

}  // namespace srs::student

#endif  // STUDENT_RECORD_SYSTEM_STUDENT_PROFILES_H
