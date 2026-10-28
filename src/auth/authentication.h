#ifndef STUDENT_RECORD_SYSTEM_AUTH_AUTHENTICATION_H
#define STUDENT_RECORD_SYSTEM_AUTH_AUTHENTICATION_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "auth/access_control.h"
#include "core/result.h"

namespace srs::auth {

struct User {
    std::string username;
    std::string password;
    Role role = Role::Student;
};

struct Session {
    std::string id;
    std::string username;
};

class AuthenticationService {
public:
    explicit AuthenticationService(std::vector<User> registered_users);

    std::unique_ptr<Session> login(
        const std::string& username,
        const std::string& password);

    bool logout(const std::string& session_id);

    bool is_authenticated(const std::string& session_id) const;

    bool is_registered(const std::string& username) const;

    // Deactivating a user also ends all of their sessions.
    bool set_user_active(const std::string& username, bool active);

    // --- account primitives used by UserAccountService (issue #5) ---
    srs::core::Result<void> add_user(const User& user);

    srs::core::Result<void> set_role(const std::string& username, Role role);

    srs::core::Result<void> set_password(
        const std::string& username,
        const std::string& new_password);

    bool check_password(
        const std::string& username,
        const std::string& password) const;

    srs::core::Result<std::string> username_for_session(
        const std::string& session_id) const;

    srs::core::Result<Role> role_of(const std::string& username) const;

    void end_sessions_for(const std::string& username);

private:
    std::unordered_map<std::string, std::string> password_hashes_;
    std::unordered_map<std::string, bool> active_users_;
    std::unordered_map<std::string, Role> roles_;
    std::unordered_map<std::string, Session> sessions_;
    unsigned long long next_session_id_ = 1;
};

}  // namespace srs::auth

#endif  // STUDENT_RECORD_SYSTEM_AUTH_AUTHENTICATION_H
