#ifndef STUDENT_RECORD_SYSTEM_CORE_RESULT_H
#define STUDENT_RECORD_SYSTEM_CORE_RESULT_H

#include "error_codes.h"

namespace srs::core {

template <typename T>
class Result;

template <>
class Result<void> {
public:
    static Result<void> success() {
        return Result<void>(true, ErrorCode::INTERNAL_ERROR);
    }

    static Result<void> failure(ErrorCode error) {
        return Result<void>(false, error);
    }

    explicit operator bool() const {
        return success_;
    }

    ErrorCode error() const {
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
