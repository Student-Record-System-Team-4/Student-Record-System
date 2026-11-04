#include "auth/authentication.h"

#include <string>
#include <utility>

#include "auth/password_hasher.h"
#include "core/validator.h"
#include "persistence/repositories.h"

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
            roles_[username] = user.role;
        }
    }

}

AuthenticationService::AuthenticationService(
    std::vector<User> registered_users,
    srs::persistence::Database& database)
    : AuthenticationService(registered_users) {
    database_ = &database;
    srs::persistence::UserRepository repository(database);
    const auto stored = repository.all();
    if (stored && !stored.value()->empty()) {
        password_hashes_.clear();
        active_users_.clear();
        roles_.clear();
        for (const auto& user : *stored.value()) {
            password_hashes_[user.username] = user.password_hash;
            active_users_[user.username] = user.active;
            roles_[user.username] =
                user.role == "Admin" ? Role::Admin :
                user.role == "Faculty" ? Role::Faculty : Role::Student;
        }
    } else {
        for (const auto& user : registered_users) {
            const auto hash = password_hashes_.find(user.username);
            if (hash != password_hashes_.end()) {
                repository.save({user.username, hash->second,
                    user.role == Role::Admin ? "Admin" :
                    user.role == Role::Faculty ? "Faculty" : "Student", true});
            }
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
    if (database_) {
        srs::persistence::UserRepository repository(*database_);
        const auto current = repository.find(username);
        if (current) repository.save({username, current.value()->password_hash,
            roles_[username] == Role::Admin ? "Admin" :
            roles_[username] == Role::Faculty ? "Faculty" : "Student", active});
    }
    if (!active) {
        end_sessions_for(username);
    }
    return true;
}

srs::core::Result<void> AuthenticationService::add_user(const User& user) {
    using srs::core::ErrorCode;
    using srs::core::Result;

    if (!srs::core::Validator::validate_username(user.username)) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }
    if (is_registered(user.username)) {
        return Result<void>::failure(ErrorCode::ALREADY_EXISTS);
    }
    const auto password_hash = PasswordHasher::hash_password(user.password);
    if (!password_hash) {
        return Result<void>::failure(password_hash.error());
    }
    password_hashes_[user.username] = *password_hash.value();
    active_users_[user.username] = true;
    roles_[user.username] = user.role;
    if (database_) {
        srs::persistence::UserRepository repository(*database_);
        repository.save({user.username, *password_hash.value(),
            user.role == Role::Admin ? "Admin" :
            user.role == Role::Faculty ? "Faculty" : "Student", true});
    }
    return Result<void>::success();
}

srs::core::Result<void> AuthenticationService::set_role(
    const std::string& username,
    Role role) {
    const auto entry = roles_.find(username);
    if (entry == roles_.end()) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::NOT_FOUND);
    }
    entry->second = role;
    if (database_) {
        srs::persistence::UserRepository repository(*database_);
        const auto current = repository.find(username);
        if (current) {
            repository.save({username, current.value()->password_hash,
                role == Role::Admin ? "Admin" :
                role == Role::Faculty ? "Faculty" : "Student",
                current.value()->active});
        }
    }
    return srs::core::Result<void>::success();
}

srs::core::Result<void> AuthenticationService::set_password(
    const std::string& username,
    const std::string& new_password) {
    if (!is_registered(username)) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::NOT_FOUND);
    }
    const auto password_hash = PasswordHasher::hash_password(new_password);
    if (!password_hash) {
        return srs::core::Result<void>::failure(password_hash.error());
    }
    password_hashes_[username] = *password_hash.value();
    if (database_) {
        srs::persistence::UserRepository repository(*database_);
        const auto current = repository.find(username);
        if (current) repository.save({username, *password_hash.value(),
            current.value()->role, current.value()->active});
    }
    return srs::core::Result<void>::success();
}

bool AuthenticationService::check_password(
    const std::string& username,
    const std::string& password) const {
    const auto user = password_hashes_.find(username);
    if (user == password_hashes_.end()) {
        return false;
    }
    const auto matches = PasswordHasher::verify_password(password, user->second);
    return matches && *matches.value();
}

srs::core::Result<std::string> AuthenticationService::username_for_session(
    const std::string& session_id) const {
    const auto session = sessions_.find(session_id);
    if (session == sessions_.end()) {
        return srs::core::Result<std::string>::failure(
            srs::core::ErrorCode::SESSION_ERROR);
    }
    return srs::core::Result<std::string>::success(session->second.username);
}

srs::core::Result<Role> AuthenticationService::role_of(
    const std::string& username) const {
    const auto entry = roles_.find(username);
    if (entry == roles_.end()) {
        return srs::core::Result<Role>::failure(
            srs::core::ErrorCode::NOT_FOUND);
    }
    return srs::core::Result<Role>::success(entry->second);
}

void AuthenticationService::end_sessions_for(const std::string& username) {
    for (auto session = sessions_.begin(); session != sessions_.end();) {
        if (session->second.username == username) {
            session = sessions_.erase(session);
        } else {
            ++session;
        }
    }
}

}  // namespace srs::auth
