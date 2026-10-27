#include "auth/authentication.h"

#include <string>
#include <utility>

#include "auth/password_hasher.h"
#include "core/validator.h"

namespace srs::auth {

AuthenticationService::AuthenticationService(
    std::vector<User> registered_users) {
    for (auto& user : registered_users) {
        if (!srs::core::Validator::validate_username(user.username)) {
            continue;
        }
        const auto password_hash = PasswordHasher::hash_password(user.password);
        if (password_hash) {
            const std::string username = user.username;
            password_hashes_[username] =
                *password_hash.value();
            active_users_[username] = true;
        }
    }
}

std::unique_ptr<Session> AuthenticationService::login(
    const std::string& username,
    const std::string& password) {
    if (!srs::core::Validator::validate_username(username) ||
        !srs::core::Validator::validate_password(password)) {
        return nullptr;
    }

    const auto user = password_hashes_.find(username);
    if (user == password_hashes_.end()) {
        return nullptr;
    }
    if (!active_users_[username]) {
        return nullptr;
    }

    const auto password_matches =
        PasswordHasher::verify_password(password, user->second);
    if (!password_matches || !*password_matches.value()) {
        return nullptr;
    }

    Session session{
        "session-" + std::to_string(next_session_id_++),
        std::string(username)};
    sessions_.emplace(session.id, session);
    return std::unique_ptr<Session>(new Session(std::move(session)));
}

bool AuthenticationService::logout(const std::string& session_id) {
    return sessions_.erase(session_id) > 0;
}

bool AuthenticationService::is_authenticated(
    const std::string& session_id) const {
    return sessions_.find(session_id) != sessions_.end();
}

bool AuthenticationService::is_registered(const std::string& username) const {
    return password_hashes_.find(username) != password_hashes_.end();
}

bool AuthenticationService::set_user_active(
    const std::string& username,
    bool active) {
    const auto user = active_users_.find(username);
    if (user == active_users_.end()) {
        return false;
    }
    user->second = active;
    return true;
}

}  // namespace srs::auth
