#include "persistence/repositories.h"

#include <sqlite3.h>

#include "core/error_codes.h"

namespace srs::persistence {
namespace {

using srs::core::ErrorCode;
using srs::core::Result;

struct Statement {
    sqlite3_stmt* value = nullptr;

    ~Statement() {
        if (value != nullptr) {
            sqlite3_finalize(value);
        }
    }
};

bool prepare(Database& database, const char* sql, Statement& statement) {
    return sqlite3_prepare_v2(
               database.handle(),
               sql,
               -1,
               &statement.value,
               nullptr) == SQLITE_OK;
}

void text(sqlite3_stmt* statement, int index, const std::string& value) {
    sqlite3_bind_text(
        statement,
        index,
        value.c_str(),
        -1,
        SQLITE_TRANSIENT);
}

void integer(sqlite3_stmt* statement, int index, int value) {
    sqlite3_bind_int(statement, index, value);
}

void real(sqlite3_stmt* statement, int index, double value) {
    sqlite3_bind_double(statement, index, value);
}

std::string column_text(sqlite3_stmt* statement, int index) {
    const auto* value = sqlite3_column_text(statement, index);

    return value == nullptr
        ? std::string{}
        : reinterpret_cast<const char*>(value);
}

Result<void> step(Database& database, sqlite3_stmt* statement) {
    const int result = sqlite3_step(statement);

    if (result == SQLITE_DONE) {
        return Result<void>::success();
    }

    if (result == SQLITE_CONSTRAINT) {
        return Result<void>::failure(ErrorCode::ALREADY_EXISTS);
    }

    return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
}

}  // namespace

// -----------------------------------------------------------------------------
// UserRepository
// -----------------------------------------------------------------------------

Result<void> UserRepository::save(const UserRow& row) {
    Statement s;

    if (!prepare(
            database_,
            "INSERT INTO users(username,password_hash,role,active) "
            "VALUES(?,?,?,?) "
            "ON CONFLICT(username) DO UPDATE SET "
            "password_hash=excluded.password_hash,"
            "role=excluded.role,"
            "active=excluded.active;",
            s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, row.username);
    text(s.value, 2, row.password_hash);
    text(s.value, 3, row.role);
    integer(s.value, 4, row.active ? 1 : 0);

    return step(database_, s.value);
}

Result<void> UserRepository::remove(const std::string& username) {
    Statement s;

    if (!prepare(database_, "DELETE FROM users WHERE username = ?;", s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, username);

    return step(database_, s.value);
}

Result<UserRow> UserRepository::find(const std::string& username) const {
    Statement s;

    if (!prepare(
            database_,
            "SELECT username,password_hash,role,active "
            "FROM users WHERE username = ?;",
            s)) {
        return Result<UserRow>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, username);

    if (sqlite3_step(s.value) != SQLITE_ROW) {
        return Result<UserRow>::failure(ErrorCode::NOT_FOUND);
    }

    UserRow row;
    row.username = column_text(s.value, 0);
    row.password_hash = column_text(s.value, 1);
    row.role = column_text(s.value, 2);
    row.active = sqlite3_column_int(s.value, 3) != 0;

    return Result<UserRow>::success(row);
}

Result<std::vector<UserRow>> UserRepository::all() const {
    Statement s;

    if (!prepare(
            database_,
            "SELECT username,password_hash,role,active "
            "FROM users ORDER BY username;",
            s)) {
        return Result<std::vector<UserRow>>::failure(
            ErrorCode::INTERNAL_ERROR);
    }

    std::vector<UserRow> rows;

    while (sqlite3_step(s.value) == SQLITE_ROW) {
        UserRow row;
        row.username = column_text(s.value, 0);
        row.password_hash = column_text(s.value, 1);
        row.role = column_text(s.value, 2);
        row.active = sqlite3_column_int(s.value, 3) != 0;
        rows.push_back(row);
    }

    return Result<std::vector<UserRow>>::success(rows);
}

// -----------------------------------------------------------------------------
// StudentRepository
// -----------------------------------------------------------------------------

Result<void> StudentRepository::save(const StudentRow& row) {
    Statement s;

    if (!prepare(
            database_,
            "INSERT INTO student_profiles("
            "srn,username,name,date_of_birth,contact,programme,semester,active"
            ") VALUES(?,?,?,?,?,?,?,?) "
            "ON CONFLICT(srn) DO UPDATE SET "
            "username=excluded.username,"
            "name=excluded.name,"
            "date_of_birth=excluded.date_of_birth,"
            "contact=excluded.contact,"
            "programme=excluded.programme,"
            "semester=excluded.semester,"
            "active=excluded.active;",
            s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, row.srn);
    text(s.value, 2, row.username);
    text(s.value, 3, row.name);
    text(s.value, 4, row.date_of_birth);
    text(s.value, 5, row.contact);
    text(s.value, 6, row.programme);
    integer(s.value, 7, row.semester);
    integer(s.value, 8, row.active ? 1 : 0);

    return step(database_, s.value);
}

Result<StudentRow> StudentRepository::find(const std::string& srn) const {
    Statement s;

    if (!prepare(
            database_,
            "SELECT srn,username,name,date_of_birth,contact,programme,"
            "semester,active FROM student_profiles WHERE srn = ?;",
            s)) {
        return Result<StudentRow>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, srn);

    if (sqlite3_step(s.value) != SQLITE_ROW) {
        return Result<StudentRow>::failure(ErrorCode::NOT_FOUND);
    }

    StudentRow row;
    row.srn = column_text(s.value, 0);
    row.username = column_text(s.value, 1);
    row.name = column_text(s.value, 2);
    row.date_of_birth = column_text(s.value, 3);
    row.contact = column_text(s.value, 4);
    row.programme = column_text(s.value, 5);
    row.semester = sqlite3_column_int(s.value, 6);
    row.active = sqlite3_column_int(s.value, 7) != 0;

    return Result<StudentRow>::success(row);
}

Result<std::vector<StudentRow>> StudentRepository::all() const {
    Statement s;

    if (!prepare(
            database_,
            "SELECT srn,username,name,date_of_birth,contact,programme,"
            "semester,active FROM student_profiles ORDER BY srn;",
            s)) {
        return Result<std::vector<StudentRow>>::failure(
            ErrorCode::INTERNAL_ERROR);
    }

    std::vector<StudentRow> rows;

    while (sqlite3_step(s.value) == SQLITE_ROW) {
        StudentRow row;
        row.srn = column_text(s.value, 0);
        row.username = column_text(s.value, 1);
        row.name = column_text(s.value, 2);
        row.date_of_birth = column_text(s.value, 3);
        row.contact = column_text(s.value, 4);
        row.programme = column_text(s.value, 5);
        row.semester = sqlite3_column_int(s.value, 6);
        row.active = sqlite3_column_int(s.value, 7) != 0;
        rows.push_back(row);
    }

    return Result<std::vector<StudentRow>>::success(rows);
}

// -----------------------------------------------------------------------------
// CourseRepository
// -----------------------------------------------------------------------------

Result<void> CourseRepository::save(const CourseRow& row) {
    Statement s;

    if (!prepare(
            database_,
            "INSERT INTO courses(code,title,credits,faculty_username,active) "
            "VALUES(?,?,?,?,?) "
            "ON CONFLICT(code) DO UPDATE SET "
            "title=excluded.title,"
            "credits=excluded.credits,"
            "faculty_username=excluded.faculty_username,"
            "active=excluded.active;",
            s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, row.code);
    text(s.value, 2, row.title);
    integer(s.value, 3, row.credits);
    text(s.value, 4, row.faculty_username);
    integer(s.value, 5, row.active ? 1 : 0);

    return step(database_, s.value);
}

Result<void> CourseRepository::deactivate(const std::string& code) {
    Statement s;

    if (!prepare(
            database_,
            "UPDATE courses SET active = 0 WHERE code = ?;",
            s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, code);

    return step(database_, s.value);
}

Result<CourseRow> CourseRepository::find(const std::string& code) const {
    Statement s;

    if (!prepare(
            database_,
            "SELECT code,title,credits,faculty_username,active "
            "FROM courses WHERE code = ?;",
            s)) {
        return Result<CourseRow>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, code);

    if (sqlite3_step(s.value) != SQLITE_ROW) {
        return Result<CourseRow>::failure(ErrorCode::NOT_FOUND);
    }

    CourseRow row;
    row.code = column_text(s.value, 0);
    row.title = column_text(s.value, 1);
    row.credits = sqlite3_column_int(s.value, 2);
    row.faculty_username = column_text(s.value, 3);
    row.active = sqlite3_column_int(s.value, 4) != 0;

    return Result<CourseRow>::success(row);
}

Result<std::vector<CourseRow>> CourseRepository::all() const {
    Statement s;

    if (!prepare(
            database_,
            "SELECT code,title,credits,faculty_username,active "
            "FROM courses ORDER BY code;",
            s)) {
        return Result<std::vector<CourseRow>>::failure(
            ErrorCode::INTERNAL_ERROR);
    }

    std::vector<CourseRow> rows;

    while (sqlite3_step(s.value) == SQLITE_ROW) {
        CourseRow row;
        row.code = column_text(s.value, 0);
        row.title = column_text(s.value, 1);
        row.credits = sqlite3_column_int(s.value, 2);
        row.faculty_username = column_text(s.value, 3);
        row.active = sqlite3_column_int(s.value, 4) != 0;
        rows.push_back(row);
    }

    return Result<std::vector<CourseRow>>::success(rows);
}

// -----------------------------------------------------------------------------
// EnrollmentRepository
// -----------------------------------------------------------------------------

Result<void> EnrollmentRepository::save(const EnrollmentRow& row) {
    Statement s;

    if (!prepare(
            database_,
            "INSERT INTO enrollments(srn,course_code,semester) "
            "VALUES(?,?,?);",
            s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, row.srn);
    text(s.value, 2, row.course_code);
    integer(s.value, 3, row.semester);

    return step(database_, s.value);
}

Result<void> EnrollmentRepository::remove(
    const std::string& srn,
    const std::string& course_code,
    int semester) {

    Statement s;

    if (!prepare(
            database_,
            "DELETE FROM enrollments "
            "WHERE srn = ? AND course_code = ? AND semester = ?;",
            s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, srn);
    text(s.value, 2, course_code);
    integer(s.value, 3, semester);

    return step(database_, s.value);
}

Result<std::vector<EnrollmentRow>> EnrollmentRepository::all() const {
    Statement s;

    if (!prepare(
            database_,
            "SELECT srn,course_code,semester FROM enrollments "
            "ORDER BY srn,course_code,semester;",
            s)) {
        return Result<std::vector<EnrollmentRow>>::failure(
            ErrorCode::INTERNAL_ERROR);
    }

    std::vector<EnrollmentRow> rows;

    while (sqlite3_step(s.value) == SQLITE_ROW) {
        EnrollmentRow row;
        row.srn = column_text(s.value, 0);
        row.course_code = column_text(s.value, 1);
        row.semester = sqlite3_column_int(s.value, 2);
        rows.push_back(row);
    }

    return Result<std::vector<EnrollmentRow>>::success(rows);
}

// -----------------------------------------------------------------------------
// GradeRepository
// -----------------------------------------------------------------------------

Result<void> GradeRepository::save(const GradeRow& row) {
    Statement s;

    if (!prepare(
            database_,
            "INSERT INTO grades("
            "srn,course_code,semester,letter,has_marks,marks,grade_points"
            ") VALUES(?,?,?,?,?,?,?) "
            "ON CONFLICT(srn,course_code,semester) DO UPDATE SET "
            "letter=excluded.letter,"
            "has_marks=excluded.has_marks,"
            "marks=excluded.marks,"
            "grade_points=excluded.grade_points;",
            s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, row.srn);
    text(s.value, 2, row.course_code);
    integer(s.value, 3, row.semester);
    text(s.value, 4, row.letter);
    integer(s.value, 5, row.has_marks ? 1 : 0);
    real(s.value, 6, row.marks);
    real(s.value, 7, row.grade_points);

    return step(database_, s.value);
}

Result<std::vector<GradeRow>> GradeRepository::all() const {
    Statement s;

    if (!prepare(
            database_,
            "SELECT srn,course_code,semester,letter,has_marks,marks,"
            "grade_points FROM grades ORDER BY srn,course_code,semester;",
            s)) {
        return Result<std::vector<GradeRow>>::failure(
            ErrorCode::INTERNAL_ERROR);
    }

    std::vector<GradeRow> rows;

    while (sqlite3_step(s.value) == SQLITE_ROW) {
        GradeRow row;
        row.srn = column_text(s.value, 0);
        row.course_code = column_text(s.value, 1);
        row.semester = sqlite3_column_int(s.value, 2);
        row.letter = column_text(s.value, 3);
        row.has_marks = sqlite3_column_int(s.value, 4) != 0;
        row.marks = sqlite3_column_double(s.value, 5);
        row.grade_points = sqlite3_column_double(s.value, 6);
        rows.push_back(row);
    }

    return Result<std::vector<GradeRow>>::success(rows);
}

// -----------------------------------------------------------------------------
// AttendanceRepository
// -----------------------------------------------------------------------------

Result<void> AttendanceRepository::save(const AttendanceRow& row) {
    Statement s;

    if (!prepare(
            database_,
            "INSERT INTO attendance("
            "srn,course_code,semester,date,session_details,status"
            ") VALUES(?,?,?,?,?,?) "
            "ON CONFLICT(srn,course_code,semester,date,session_details) "
            "DO UPDATE SET status=excluded.status;",
            s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, row.srn);
    text(s.value, 2, row.course_code);
    integer(s.value, 3, row.semester);
    text(s.value, 4, row.date);
    text(s.value, 5, row.session_details);
    text(s.value, 6, row.status);

    return step(database_, s.value);
}

Result<std::vector<AttendanceRow>> AttendanceRepository::all() const {
    Statement s;

    if (!prepare(
            database_,
            "SELECT srn,course_code,semester,date,session_details,status "
            "FROM attendance "
            "ORDER BY srn,course_code,semester,date,session_details;",
            s)) {
        return Result<std::vector<AttendanceRow>>::failure(
            ErrorCode::INTERNAL_ERROR);
    }

    std::vector<AttendanceRow> rows;

    while (sqlite3_step(s.value) == SQLITE_ROW) {
        AttendanceRow row;
        row.srn = column_text(s.value, 0);
        row.course_code = column_text(s.value, 1);
        row.semester = sqlite3_column_int(s.value, 2);
        row.date = column_text(s.value, 3);
        row.session_details = column_text(s.value, 4);
        row.status = column_text(s.value, 5);
        rows.push_back(row);
    }

    return Result<std::vector<AttendanceRow>>::success(rows);
}

// -----------------------------------------------------------------------------
// AuditRepository
// -----------------------------------------------------------------------------

Result<void> AuditRepository::append(const AuditRow& row) {
    Statement s;

    if (!prepare(
            database_,
            "INSERT INTO audit_log("
            "username,action,entity,entity_id,details"
            ") VALUES(?,?,?,?,?);",
            s)) {
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    }

    text(s.value, 1, row.username);
    text(s.value, 2, row.action);
    text(s.value, 3, row.entity);
    text(s.value, 4, row.entity_id);
    text(s.value, 5, row.details);

    return step(database_, s.value);
}

}  // namespace srs::persistence
