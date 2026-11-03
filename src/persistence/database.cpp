#include "persistence/database.h"

namespace srs::persistence {

namespace {
const char* schema = R"sql(
PRAGMA foreign_keys = ON;
CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS roles (
    name TEXT PRIMARY KEY
);
INSERT OR IGNORE INTO roles(name) VALUES ('Student'), ('Faculty'), ('Admin');
CREATE TABLE IF NOT EXISTS users (
    username TEXT PRIMARY KEY,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL REFERENCES roles(name),
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1))
);
CREATE TABLE IF NOT EXISTS student_profiles (
    srn TEXT PRIMARY KEY,
    username TEXT NOT NULL UNIQUE REFERENCES users(username),
    name TEXT NOT NULL,
    date_of_birth TEXT NOT NULL,
    contact TEXT NOT NULL,
    programme TEXT NOT NULL,
    semester INTEGER NOT NULL CHECK(semester > 0),
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1))
);
CREATE TABLE IF NOT EXISTS courses (
    code TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    credits INTEGER NOT NULL CHECK(credits BETWEEN 1 AND 10),
    faculty_username TEXT NOT NULL REFERENCES users(username),
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0, 1))
);
CREATE TABLE IF NOT EXISTS enrollments (
    srn TEXT NOT NULL REFERENCES student_profiles(srn),
    course_code TEXT NOT NULL REFERENCES courses(code),
    semester INTEGER NOT NULL CHECK(semester BETWEEN 1 AND 12),
    PRIMARY KEY(srn, course_code, semester)
);
CREATE TABLE IF NOT EXISTS grades (
    srn TEXT NOT NULL,
    course_code TEXT NOT NULL,
    semester INTEGER NOT NULL,
    has_marks INTEGER NOT NULL CHECK(has_marks IN (0,1)),
    marks REAL NOT NULL DEFAULT 0 CHECK(marks BETWEEN 0 AND 100),
    letter TEXT NOT NULL,
    grade_points REAL NOT NULL CHECK(grade_points >= 0),
    PRIMARY KEY(srn, course_code, semester),
    FOREIGN KEY(srn, course_code, semester)
        REFERENCES enrollments(srn, course_code, semester)
);
CREATE TABLE IF NOT EXISTS attendance (
    srn TEXT NOT NULL,
    course_code TEXT NOT NULL,
    semester INTEGER NOT NULL,
    date TEXT NOT NULL,
    session_details TEXT NOT NULL,
    status TEXT NOT NULL CHECK(status IN ('Present','Absent','Late')),
    PRIMARY KEY(srn, course_code, semester, date, session_details),
    FOREIGN KEY(srn, course_code, semester)
        REFERENCES enrollments(srn, course_code, semester)
);
CREATE TABLE IF NOT EXISTS audit_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT REFERENCES users(username),
    action TEXT NOT NULL,
    entity TEXT NOT NULL,
    entity_id TEXT,
    details TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
INSERT OR IGNORE INTO schema_migrations(version) VALUES (1);
)sql";
}

Database::Database(const std::string& path) : path_(path) {
    if (sqlite3_open(path.c_str(), &handle_) != SQLITE_OK) {
        last_error_ = handle_ ? sqlite3_errmsg(handle_) : "Unable to open database";
        if (handle_) sqlite3_close(handle_);
        handle_ = nullptr;
        return;
    }
    if (!execute(schema)) {
        sqlite3_close(handle_);
        handle_ = nullptr;
    }
}

Database::~Database() {
    if (handle_) sqlite3_close(handle_);
}

bool Database::is_open() const noexcept { return handle_ != nullptr; }
const std::string& Database::path() const noexcept { return path_; }
const std::string& Database::last_error() const noexcept { return last_error_; }
sqlite3* Database::handle() const noexcept { return handle_; }

srs::core::Result<void> Database::execute(const std::string& sql) {
    if (!handle_) return srs::core::Result<void>::failure(srs::core::ErrorCode::INTERNAL_ERROR);
    char* error = nullptr;
    const int result = sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, &error);
    if (result != SQLITE_OK) {
        last_error_ = error ? error : sqlite3_errmsg(handle_);
        sqlite3_free(error);
        return srs::core::Result<void>::failure(srs::core::ErrorCode::INTERNAL_ERROR);
    }
    return srs::core::Result<void>::success();
}

srs::core::Result<void> Database::begin_transaction() { return execute("BEGIN TRANSACTION;"); }
srs::core::Result<void> Database::commit() { return execute("COMMIT;"); }
srs::core::Result<void> Database::rollback() { return execute("ROLLBACK;"); }

Transaction::Transaction(Database& database) : database_(database) {
    started_ = database_.begin_transaction().has_value();
}
Transaction::~Transaction() {
    if (started_ && !committed_) database_.rollback();
}
bool Transaction::started() const noexcept { return started_; }
srs::core::Result<void> Transaction::commit() {
    if (!started_ || committed_) return srs::core::Result<void>::failure(srs::core::ErrorCode::INTERNAL_ERROR);
    auto result = database_.commit();
    if (result) committed_ = true;
    return result;
}

}  // namespace srs::persistence
