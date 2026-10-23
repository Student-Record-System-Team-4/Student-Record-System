
#ifndef STUDENT_RECORD_SYSTEM_CORE_RESULT_H
#define STUDENT_RECORD_SYSTEM_CORE_RESULT_H

#include <optional>
#include <utility>
#include "error_codes.h"

namespace srs::core {

template <typename T>
class Result {
public:
    static Result success(T value) {
        return Result(std::move(value));
    }

    static Result failure(ErrorCode error) {
        return Result(error);
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
    static Result success() {
        return Result(ErrorCode::NONE);
    }

    static Result failure(ErrorCode error) {
        return Result(error);
    }

    bool has_value() const noexcept {
        return error_ == ErrorCode::NONE;
    }

    explicit operator bool() const noexcept {
        return has_value();
    }

    ErrorCode error() const noexcept {
        return error_;
    }

private:
    explicit Result(ErrorCode error) : error_(error) {}
    ErrorCode error_;
};

}  // namespace srs::core

#endif
