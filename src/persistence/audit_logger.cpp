#include "persistence/audit_logger.h"

#include <sqlite3.h>

namespace srs::persistence {

AuditLogger::AuditLogger(Database& database)
    : database_(database) {
}

srs::core::Result<void> AuditLogger::log(
    const std::string& username,
    const std::string& action,
    const std::string& entity,
    const std::string& entity_id,
    const std::string& details
) {
    if (!database_.is_open() ||
        action.empty() ||
        entity.empty()) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INVALID_INPUT
        );
    }

    const char* sql =
        "INSERT INTO audit_records "
        "(username, action, entity, entity_id, details) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* statement = nullptr;

    const int prepare_result = sqlite3_prepare_v2(
        database_.handle(),
        sql,
        -1,
        &statement,
        nullptr
    );

    if (prepare_result != SQLITE_OK) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INTERNAL_ERROR
        );
    }

    // Username is optional. Store SQL NULL when it is empty.
    if (username.empty()) {
        sqlite3_bind_null(statement, 1);
    } else {
        sqlite3_bind_text(
            statement,
            1,
            username.c_str(),
            -1,
            SQLITE_TRANSIENT
        );
    }

    sqlite3_bind_text(
        statement,
        2,
        action.c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_text(
        statement,
        3,
        entity.c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    if (entity_id.empty()) {
        sqlite3_bind_null(statement, 4);
    } else {
        sqlite3_bind_text(
            statement,
            4,
            entity_id.c_str(),
            -1,
            SQLITE_TRANSIENT
        );
    }

    if (details.empty()) {
        sqlite3_bind_null(statement, 5);
    } else {
        sqlite3_bind_text(
            statement,
            5,
            details.c_str(),
            -1,
            SQLITE_TRANSIENT
        );
    }

    const int step_result = sqlite3_step(statement);
    sqlite3_finalize(statement);

    if (step_result != SQLITE_DONE) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INTERNAL_ERROR
        );
    }

    return srs::core::Result<void>::success();
}

}  // namespace srs::persistence
