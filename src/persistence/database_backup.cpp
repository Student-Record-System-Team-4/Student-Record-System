#include "persistence/database_backup.h"

#include <sqlite3.h>

namespace srs::persistence {

namespace {

srs::core::Result<void> copy_database(
    sqlite3* source,
    sqlite3* destination
) {
    if (source == nullptr || destination == nullptr) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INTERNAL_ERROR
        );
    }

    sqlite3_backup* backup = sqlite3_backup_init(
        destination,
        "main",
        source,
        "main"
    );

    if (backup == nullptr) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INTERNAL_ERROR
        );
    }

    int result = SQLITE_OK;
    int retries = 0;

    constexpr int MAX_RETRIES = 50;
    constexpr int RETRY_DELAY_MS = 100;

    do {
        result = sqlite3_backup_step(backup, 128);

        if (result == SQLITE_BUSY || result == SQLITE_LOCKED) {
            ++retries;
            sqlite3_sleep(RETRY_DELAY_MS);
        } else {
            retries = 0;
        }

    } while (
        result == SQLITE_OK ||
        ((result == SQLITE_BUSY || result == SQLITE_LOCKED) &&
         retries < MAX_RETRIES)
    );

    const int finish_result = sqlite3_backup_finish(backup);

    if (result != SQLITE_DONE || finish_result != SQLITE_OK) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INTERNAL_ERROR
        );
    }

    return srs::core::Result<void>::success();
}

}  // namespace

srs::core::Result<void> DatabaseBackup::create_backup(
    Database& database,
    const std::string& backup_path
) {
    if (!database.is_open() || backup_path.empty()) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INVALID_INPUT
        );
    }

    if (backup_path == database.path()) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INVALID_INPUT
        );
    }

    sqlite3* backup_database = nullptr;

    const int open_result = sqlite3_open(
        backup_path.c_str(),
        &backup_database
    );

    if (open_result != SQLITE_OK) {
        if (backup_database != nullptr) {
            sqlite3_close(backup_database);
        }

        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INTERNAL_ERROR
        );
    }

    sqlite3_busy_timeout(backup_database, 5000);

    auto result = copy_database(
        database.handle(),
        backup_database
    );

    sqlite3_close(backup_database);

    return result;
}

srs::core::Result<void> DatabaseBackup::restore_backup(
    Database& database,
    const std::string& backup_path
) {
    if (!database.is_open() || backup_path.empty()) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INVALID_INPUT
        );
    }

    if (backup_path == database.path()) {
        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INVALID_INPUT
        );
    }

    sqlite3* backup_database = nullptr;

    const int open_result = sqlite3_open_v2(
        backup_path.c_str(),
        &backup_database,
        SQLITE_OPEN_READONLY,
        nullptr
    );

    if (open_result != SQLITE_OK) {
        if (backup_database != nullptr) {
            sqlite3_close(backup_database);
        }

        return srs::core::Result<void>::failure(
            srs::core::ErrorCode::INTERNAL_ERROR
        );
    }

    auto result = copy_database(
        backup_database,
        database.handle()
    );

    sqlite3_close(backup_database);

    return result;
}

}  // namespace srs::persistence
