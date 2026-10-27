#ifndef STUDENT_RECORD_SYSTEM_AUTH_PASSWORD_HASHER_H
#define STUDENT_RECORD_SYSTEM_AUTH_PASSWORD_HASHER_H

#include <string>

#include "core/result.h"

namespace srs::auth {

class PasswordHasher {
public:
    static srs::core::Result<std::string> hash_password(
        const std::string& password);

    static srs::core::Result<bool> verify_password(
        const std::string& password,
        const std::string& stored_hash);
};

}  // namespace srs::auth

#endif  // STUDENT_RECORD_SYSTEM_AUTH_PASSWORD_HASHER_H
