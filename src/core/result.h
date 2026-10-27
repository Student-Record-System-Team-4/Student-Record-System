#ifndef STUDENT_RECORD_SYSTEM_CORE_RESULT_H
#define STUDENT_RECORD_SYSTEM_CORE_RESULT_H

#include <optional>
#include <utility>

#include "error_codes.h"

namespace srs::core {

template <typename T>
class Result {
public:
    static Result<T> success(T value) {
        return Result<T>(std::move(value));
    }

    static Result<T> failure(ErrorCode error) {
        return Result<T>(error);
    }

    bool has_value() const noexcept {
        return value_.has_value();
    }

    explicit operator bool() const noexcept {
        return has_value();
    }

    const T* value() const noexcept {
        return value_ ? &(*value_) : nullptr;
    }

    T* value() noexcept {
        return value_ ? &(*value_) : nullptr;
    }

    ErrorCode error() const noexcept {
        return error_;
    }

private:
    explicit Result(T value)
        : value_(std::move(value)), error_(ErrorCode::NONE) {}

    explicit Result(ErrorCode error)
        : value_(std::nullopt), error_(error) {}

    std::optional<T> value_;
    ErrorCode error_;
};

template <>
class Result<void> {
public:
    static Result<void> success() {
        return Result<void>(true, ErrorCode::NONE);
    }

    static Result<void> failure(ErrorCode error) {
        return Result<void>(false, error);
    }

    bool has_value() const noexcept {
        return success_;
    }

    explicit operator bool() const noexcept {
        return has_value();
    }

    ErrorCode error() const noexcept {
        return error_;
    }

private:
    Result(bool success, ErrorCode error)
        : success_(success), error_(error) {}

    bool success_;
    ErrorCode error_;
};

}  // namespace srs::core

#endif  // STUDENT_RECORD_SYSTEM_CORE_RESULT_H