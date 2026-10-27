#ifndef STUDENT_RECORD_SYSTEM_AUTH_AUTHENTICATION_H
#define STUDENT_RECORD_SYSTEM_AUTH_AUTHENTICATION_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace srs::auth {

struct User {
    std::string username;
    std::string password;
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

    bool set_user_active(const std::string& username, bool active);

private:
    std::unordered_map<std::string, std::string> password_hashes_;
    std::unordered_map<std::string, bool> active_users_;
    std::unordered_map<std::string, Session> sessions_;
    unsigned long long next_session_id_ = 1;
};

}  // namespace srs::auth

#endif  // STUDENT_RECORD_SYSTEM_AUTH_AUTHENTICATION_H
