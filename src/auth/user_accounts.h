#ifndef STUDENT_RECORD_SYSTEM_AUTH_USER_ACCOUNTS_H
#define STUDENT_RECORD_SYSTEM_AUTH_USER_ACCOUNTS_H

#include <string>

#include "auth/access_control.h"
#include "auth/authentication.h"
#include "core/result.h"

namespace srs::auth {

// SRS-F-003, F-004, F-005. Every call is identified by the caller's session id
// and is checked against the role -> permission matrix before it does anything.
class UserAccountService {
public:
    explicit UserAccountService(AuthenticationService& authentication);

    // OK if the session is valid and its role holds `permission`;
    // SESSION_ERROR if not logged in; PERMISSION_DENIED if the role lacks it.
    srs::core::Result<void> authorize(
        const std::string& session_id,
        Permission permission) const;

    // Admin only (SRS-F-004).
    srs::core::Result<void> create_user(
        const std::string& session_id,
        const User& new_user);

    srs::core::Result<void> set_user_role(
        const std::string& session_id,
        const std::string& username,
        Role role);

    srs::core::Result<void> set_user_active(
        const std::string& session_id,
        const std::string& username,
        bool active);

    srs::core::Result<void> reset_password(
        const std::string& session_id,
        const std::string& username,
        const std::string& new_password);

    // Any logged-in user, own account only (SRS-F-005).
    srs::core::Result<void> change_password(
        const std::string& session_id,
        const std::string& current_password,
        const std::string& new_password);

private:
    // Returns the acting admin's username, or the reason access was refused.
    srs::core::Result<std::string> require_admin(
        const std::string& session_id) const;

    AuthenticationService& authentication_;
};

}  // namespace srs::auth

#endif  // STUDENT_RECORD_SYSTEM_AUTH_USER_ACCOUNTS_H
