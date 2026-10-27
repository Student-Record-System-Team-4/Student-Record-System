#include "auth/authentication.h"

#include <string>
#include <utility>

namespace srs::auth {

AuthenticationService::AuthenticationService(
    std::vector<User> registered_users) {
    for (auto& user : registered_users) {
        passwords_[std::move(user.username)] = std::move(user.password);
    }
}

std::unique_ptr<Session> AuthenticationService::login(
    const std::string& username,
    const std::string& password) {
    const auto user = passwords_.find(username);
    if (user == passwords_.end() || user->second != password) {
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

}  // namespace srs::auth
