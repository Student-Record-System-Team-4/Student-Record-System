#ifndef STUDENT_RECORD_SYSTEM_PERSISTENCE_DATABASE_H
#define STUDENT_RECORD_SYSTEM_PERSISTENCE_DATABASE_H

#include <sqlite3.h>

#include <string>

#include "core/result.h"

namespace srs::persistence {

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool is_open() const noexcept;
    const std::string& path() const noexcept;
    const std::string& last_error() const noexcept;
    sqlite3* handle() const noexcept;

    srs::core::Result<void> execute(const std::string& sql);
    srs::core::Result<void> begin_transaction();
    srs::core::Result<void> commit();
    srs::core::Result<void> rollback();

private:
    sqlite3* handle_ = nullptr;
    std::string path_;
    std::string last_error_;
};

class Transaction {
public:
    explicit Transaction(Database& database);
    ~Transaction();

    bool started() const noexcept;
    srs::core::Result<void> commit();

private:
    Database& database_;
    bool committed_ = false;
    bool started_ = false;
};

}  // namespace srs::persistence

#endif
