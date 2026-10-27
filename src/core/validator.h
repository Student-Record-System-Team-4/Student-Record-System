
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

        bool has_non_whitespace = false;

        for (char ch : username) {
            if (ch == '\0') {
                return Result<void>::failure(ErrorCode::INVALID_INPUT);
            }

            if (ch != ' ' && ch != '\t' && ch != '\n' &&
                ch != '\r' && ch != '\f' && ch != '\v') {
                has_non_whitespace = true;
            }
        }

        if (!has_non_whitespace) {
            return Result<void>::failure(ErrorCode::INVALID_INPUT);
        }

        return Result<void>::success();
    }

    static Result<void> validate_password(std::string_view password) {
        if (password.size() < MIN_PASSWORD_LENGTH) {
            return Result<void>::failure(ErrorCode::INVALID_INPUT);
        }

        bool has_non_whitespace = false;

        for (char ch : password) {
            if (ch == '\0') {
                return Result<void>::failure(ErrorCode::INVALID_INPUT);
            }

            if (ch != ' ' && ch != '\t' && ch != '\n' &&
                ch != '\r' && ch != '\f' && ch != '\v') {
                has_non_whitespace = true;
            }
        }

        if (!has_non_whitespace) {
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

        auto password_result = validate_password(password);

        if (!password_result) {
            return Result<void>::failure(password_result.error());
        }

        return Result<void>::success();
    }
};

}  // namespace srs::core

#endif  // STUDENT_RECORD_SYSTEM_CORE_VALIDATOR_H
