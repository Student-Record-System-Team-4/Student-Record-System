
#ifndef STUDENT_RECORD_SYSTEM_AUTH_PASSWORD_HASHER_H
#define STUDENT_RECORD_SYSTEM_AUTH_PASSWORD_HASHER_H

#include <string>
#include <string_view>

#include "../core/result.h"

namespace srs::auth {

class PasswordHasher {
public:
    static srs::core::Result<std::string> hash_password(
        std::string_view password);

    static srs::core::Result<bool> verify_password(
        std::string_view password,
        std::string_view stored_hash);
};

}  // namespace srs::auth

#endif
