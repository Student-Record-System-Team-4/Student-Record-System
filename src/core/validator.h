
#ifndef STUDENT_RECORD_SYSTEM_CORE_VALIDATOR_H
#define STUDENT_RECORD_SYSTEM_CORE_VALIDATOR_H

#include <cstddef>
#include <string_view>

#include "error_codes.h"
#include "result.h"

namespace srs::core {

class Validator {
public:
    static constexpr std::size_t MIN_PASSWORD_LENGTH = 8;

    static Result<void> validate_username(std::string_view username) {
        if (username.empty()) {
            return Result<void>::failure(ErrorCode::INVALID_INPUT);
        }

        return Result<void>::success();
    }

    static Result<void> validate_password(std::string_view password) {
        if (password.size() < MIN_PASSWORD_LENGTH) {
            return Result<void>::failure(ErrorCode::INVALID_INPUT);
        }

        return Result<void>::success();
    }

    static Result<void> validate_credentials(
        std::string_view username,
        std::string_view password) {

        auto username_result = validate_username(username);

        if (!username_result) {
            return Result<void>::failure(username_result.error());
        }

        return validate_password(password);
    }
};

}  // namespace srs::core

#endif  // STUDENT_RECORD_SYSTEM_CORE_VALIDATOR_H
