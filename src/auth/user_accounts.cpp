#include "auth/user_accounts.h"

namespace srs::auth {

using srs::core::ErrorCode;
using srs::core::Result;

UserAccountService::UserAccountService(AuthenticationService& authentication)
    : authentication_(authentication) {}

Result<void> UserAccountService::authorize(
    const std::string& session_id,
    Permission permission) const {
    const auto username = authentication_.username_for_session(session_id);
    if (!username) {
        return Result<void>::failure(ErrorCode::SESSION_ERROR);
    }
    const auto role = authentication_.role_of(*username.value());
    if (!role) {
        return Result<void>::failure(ErrorCode::SESSION_ERROR);
    }
    if (!AccessControl::is_allowed(*role.value(), permission)) {
        return Result<void>::failure(ErrorCode::PERMISSION_DENIED);
    }
    return Result<void>::success();
}

Result<std::string> UserAccountService::require_admin(
    const std::string& session_id) const {
    const auto allowed = authorize(session_id, Permission::ManageUsers);
    if (!allowed) {
        return Result<std::string>::failure(allowed.error());
    }
    return authentication_.username_for_session(session_id);
}

Result<void> UserAccountService::create_user(
    const std::string& session_id,
    const User& new_user) {
    const auto admin = require_admin(session_id);
    if (!admin) {
        return Result<void>::failure(admin.error());
    }
    return authentication_.add_user(new_user);
}

Result<void> UserAccountService::set_user_role(
    const std::string& session_id,
    const std::string& username,
    Role role) {
    const auto admin = require_admin(session_id);
    if (!admin) {
        return Result<void>::failure(admin.error());
    }
    if (*admin.value() == username) {
        // An admin must not be able to demote themselves.
        return Result<void>::failure(ErrorCode::PERMISSION_DENIED);
    }
    return authentication_.set_role(username, role);
}

Result<void> UserAccountService::set_user_active(
    const std::string& session_id,
    const std::string& username,
    bool active) {
    const auto admin = require_admin(session_id);
    if (!admin) {
        return Result<void>::failure(admin.error());
    }
    if (!active && *admin.value() == username) {
        // An admin must not be able to deactivate themselves.
        return Result<void>::failure(ErrorCode::PERMISSION_DENIED);
    }
    if (!authentication_.set_user_active(username, active)) {
        return Result<void>::failure(ErrorCode::NOT_FOUND);
    }
    return Result<void>::success();
}

Result<void> UserAccountService::reset_password(
    const std::string& session_id,
    const std::string& username,
    const std::string& new_password) {
    const auto admin = require_admin(session_id);
    if (!admin) {
        return Result<void>::failure(admin.error());
    }
    auto result = authentication_.set_password(username, new_password);
    if (result) {
        authentication_.end_sessions_for(username);
    }
    return result;
}

Result<void> UserAccountService::change_password(
    const std::string& session_id,
    const std::string& current_password,
    const std::string& new_password) {
    const auto username = authentication_.username_for_session(session_id);
    if (!username) {
        return Result<void>::failure(ErrorCode::SESSION_ERROR);
    }
    if (!authentication_.check_password(*username.value(), current_password)) {
        return Result<void>::failure(ErrorCode::INVALID_CREDENTIALS);
    }
    if (new_password == current_password) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }
    return authentication_.set_password(*username.value(), new_password);
}

}  // namespace srs::auth
