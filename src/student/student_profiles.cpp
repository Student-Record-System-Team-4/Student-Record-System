#include "student/student_profiles.h"

#include <algorithm>
#include <cctype>

#include "persistence/repositories.h"

namespace srs::student {
namespace {

bool non_empty_within_limit(const std::string& value, std::size_t limit) {
    return !value.empty() && value.size() <= limit;
}

std::string lowercase(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    for (char character : value) {
        result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))));
    }

    return result;
}

}  // namespace

StudentProfileService::StudentProfileService(
    srs::auth::AuthenticationService& authentication)
    : authentication_(authentication) {}

StudentProfileService::StudentProfileService(
    srs::auth::AuthenticationService& authentication,
    srs::persistence::Database& database)
    : authentication_(authentication), database_(&database) {
    srs::persistence::StudentRepository repository(*database_);

    const auto rows = repository.all();
    if (!rows) {
        return;
    }

    for (const auto& row : *rows.value()) {
        profiles_.push_back({
            row.username,
            row.name,
            row.srn,
            row.date_of_birth,
            row.phone,
            row.department,
            row.semester,
            row.active
        });
    }
}

srs::core::Result<void> StudentProfileService::register_student(
    const std::string& session_id,
    const StudentProfile& profile) {
    const auto username = authentication_.username_for_session(session_id);

    const auto role =
        username ? authentication_.role_of(*username.value())
                 : srs::core::Result<srs::auth::Role>::failure(
                       srs::core::ErrorCode::SESSION_ERROR);

    if (!role || *role.value() != srs::auth::Role::Admin ||
        !valid_profile(profile) ||
        !authentication_.is_registered(profile.username)) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INVALID_INPUT);
    }

    for (const auto& existing : profiles_) {
        if (existing.srn == profile.srn ||
            existing.username == profile.username) {
            return srs::core::Result<void>::failure(
                srs::core::ErrorCode::ALREADY_EXISTS);
        }
    }

    if (database_) {
        srs::persistence::StudentRepository repository(*database_);

        const auto saved = repository.save({
            profile.srn,
            profile.username,
            profile.name,
            profile.date_of_birth,
            profile.contact,
            profile.programme,
            profile.semester,
            profile.active
        });

        if (!saved) {
            return srs::core::Result<void>::failure(
                srs::core::ErrorCode::INTERNAL_ERROR);
        }
    }

    profiles_.push_back(profile);
    return srs::core::Result<void>::success();
}

srs::core::Result<void> StudentProfileService::update_student(
    const std::string& session_id,
    const std::string& srn,
    const StudentProfile& profile) {
    const auto username = authentication_.username_for_session(session_id);

    const auto role =
        username ? authentication_.role_of(*username.value())
                 : srs::core::Result<srs::auth::Role>::failure(
                       srs::core::ErrorCode::SESSION_ERROR);

    if (!role || *role.value() != srs::auth::Role::Admin ||
        !valid_profile(profile)) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INVALID_INPUT);
    }

    for (auto& existing : profiles_) {
        if (existing.srn != srn) {
            continue;
        }

        if (profile.srn != srn) {
            return srs::core::Result<void>::failure(
                srs::core::ErrorCode::INVALID_INPUT);
        }

        if (database_) {
            srs::persistence::StudentRepository repository(*database_);

            const auto saved = repository.save({
                profile.srn,
                profile.username,
                profile.name,
                profile.date_of_birth,
                profile.contact,
                profile.programme,
                profile.semester,
                profile.active
            });

            if (!saved) {
                return srs::core::Result<void>::failure(
                    srs::core::ErrorCode::INTERNAL_ERROR);
            }
        }

        existing = profile;
        return srs::core::Result<void>::success();
    }

    return srs::core::Result<void>::failure(
        srs::core::ErrorCode::NOT_FOUND);
}

srs::core::Result<void> StudentProfileService::deactivate_student(
    const std::string& session_id,
    const std::string& srn) {
    const auto username = authentication_.username_for_session(session_id);

    const auto role =
        username ? authentication_.role_of(*username.value())
                 : srs::core::Result<srs::auth::Role>::failure(
                       srs::core::ErrorCode::SESSION_ERROR);

    if (!role || *role.value() != srs::auth::Role::Admin) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::PERMISSION_DENIED);
    }

    for (auto& profile : profiles_) {
        if (profile.srn != srn) {
            continue;
        }

        if (database_) {
            srs::persistence::StudentRepository repository(*database_);

            const auto saved = repository.save({
                profile.srn,
                profile.username,
                profile.name,
                profile.date_of_birth,
                profile.contact,
                profile.programme,
                profile.semester,
                false
            });

            if (!saved) {
                return srs::core::Result<void>::failure(
                    srs::core::ErrorCode::INTERNAL_ERROR);
            }
        }

        profile.active = false;
        authentication_.set_user_active(profile.username, false);

        return srs::core::Result<void>::success();
    }

    return srs::core::Result<void>::failure(
        srs::core::ErrorCode::NOT_FOUND);
}

srs::core::Result<StudentProfile> StudentProfileService::view_own_profile(
    const std::string& session_id) const {
    const auto username = authentication_.username_for_session(session_id);

    if (!username) {
        return srs::core::Result<StudentProfile>::failure(
            srs::core::ErrorCode::SESSION_ERROR);
    }

    const auto role = authentication_.role_of(*username.value());

    if (!role || *role.value() != srs::auth::Role::Student) {
        return srs::core::Result<StudentProfile>::failure(
            srs::core::ErrorCode::PERMISSION_DENIED);
    }

    for (const auto& profile : profiles_) {
        if (profile.username == *username.value() && profile.active) {
            return srs::core::Result<StudentProfile>::success(profile);
        }
    }

    return srs::core::Result<StudentProfile>::failure(
        srs::core::ErrorCode::NOT_FOUND);
}

srs::core::Result<std::vector<StudentProfile>>
StudentProfileService::search_students(
    const std::string& session_id,
    const StudentSearchQuery& query) const {
    const auto username = authentication_.username_for_session(session_id);

    const auto role =
        username ? authentication_.role_of(*username.value())
                 : srs::core::Result<srs::auth::Role>::failure(
                       srs::core::ErrorCode::SESSION_ERROR);

    if (!role || *role.value() != srs::auth::Role::Admin ||
        query.semester < 0) {
        return srs::core::Result<std::vector<StudentProfile>>::failure(
            srs::core::ErrorCode::PERMISSION_DENIED);
    }

    std::vector<StudentProfile> matches;

    for (const auto& profile : profiles_) {
        if (!profile.active ||
            (!query.name.empty() &&
             !contains_case_insensitive(profile.name, query.name)) ||
            (!query.srn.empty() &&
             !contains_case_insensitive(profile.srn, query.srn)) ||
            (!query.programme.empty() &&
             !contains_case_insensitive(profile.programme, query.programme)) ||
            (query.semester != 0 && profile.semester != query.semester)) {
            continue;
        }

        matches.push_back(profile);
    }

    return srs::core::Result<std::vector<StudentProfile>>::success(
        matches);
}

bool StudentProfileService::valid_profile(const StudentProfile& profile) {
    if (!non_empty_within_limit(profile.username, 64) ||
        !srs::core::Validator::validate_username(profile.username) ||
        !non_empty_within_limit(profile.name, 100) ||
        !non_empty_within_limit(profile.srn, 32) ||
        !non_empty_within_limit(profile.date_of_birth, 10) ||
        !non_empty_within_limit(profile.contact, 100) ||
        !non_empty_within_limit(profile.programme, 100) ||
        profile.semester < 1) {
        return false;
    }

    for (char character : profile.srn) {
        if (!std::isalnum(static_cast<unsigned char>(character)) &&
            character != '-') {
            return false;
        }
    }

    return true;
}

bool StudentProfileService::contains_case_insensitive(
    const std::string& value,
    const std::string& query) {
    return lowercase(value).find(lowercase(query)) != std::string::npos;
}

}  // namespace srs::student
