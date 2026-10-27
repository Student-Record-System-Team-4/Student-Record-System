#ifndef STUDENT_RECORD_SYSTEM_CORE_VALIDATOR_H
#define STUDENT_RECORD_SYSTEM_CORE_VALIDATOR_H

#include <cstddef>
#include <string>

#include "error_codes.h"
#include "result.h"

namespace srs::core {

class Validator {
public:
    static constexpr std::size_t MAX_USERNAME_LENGTH = 64;
    static constexpr std::size_t MIN_PASSWORD_LENGTH = 8;
    static constexpr std::size_t MAX_PASSWORD_LENGTH = 128;

    static Result<void> validate_username(const std::string& username) {
        if (username.empty() || username.size() > MAX_USERNAME_LENGTH) {
            return Result<void>::failure(ErrorCode::INVALID_INPUT);
        }

        for (char ch : username) {
            const bool is_letter =
                (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
            const bool is_digit = ch >= '0' && ch <= '9';
            if (!is_letter && !is_digit && ch != '_' && ch != '-') {
                return Result<void>::failure(ErrorCode::INVALID_INPUT);
            }
        }

        return Result<void>::success();
    }

    static Result<void> validate_password(const std::string& password) {
        if (password.size() < MIN_PASSWORD_LENGTH ||
            password.size() > MAX_PASSWORD_LENGTH) {
            return Result<void>::failure(ErrorCode::INVALID_INPUT);
        }

        bool has_non_whitespace = false;

        for (char ch : password) {
            if (ch < 33 || ch > 126) {
                return Result<void>::failure(ErrorCode::INVALID_INPUT);
            }

            if (ch != ' ') {
                has_non_whitespace = true;
            }
        }

        if (!has_non_whitespace) {
            return Result<void>::failure(ErrorCode::INVALID_INPUT);
        }

        return Result<void>::success();
    }

    static Result<void> validate_credentials(
        const std::string& username,
        const std::string& password) {

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