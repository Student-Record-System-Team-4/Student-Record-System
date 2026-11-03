#include "persistence/repositories.h"

namespace srs::persistence {
namespace {

using srs::core::ErrorCode;
using srs::core::Result;

struct Statement {
    sqlite3_stmt* value = nullptr;
    ~Statement() { if (value) sqlite3_finalize(value); }
};

Result<void> prepare(Database& db, const char* sql, Statement& statement) {
    if (!db.is_open() || sqlite3_prepare_v2(db.handle(), sql, -1, &statement.value, nullptr) != SQLITE_OK)
        return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    return Result<void>::success();
}
void text(sqlite3_stmt* s, int index, const std::string& value) {
    sqlite3_bind_text(s, index, value.c_str(), -1, SQLITE_TRANSIENT);
}
Result<void> step(Database& db, sqlite3_stmt* s) {
    if (sqlite3_step(s) == SQLITE_DONE) return Result<void>::success();
    return Result<void>::failure(sqlite3_errcode(db.handle()) == SQLITE_CONSTRAINT
        ? ErrorCode::ALREADY_EXISTS : ErrorCode::INTERNAL_ERROR);
}
}

Result<void> UserRepository::save(const UserRow& row) {
    Statement s; if (!prepare(database_, "INSERT INTO users(username,password_hash,role,active) VALUES(?,?,?,?) ON CONFLICT(username) DO UPDATE SET password_hash=excluded.password_hash,role=excluded.role,active=excluded.active;", s)) return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    text(s.value,1,row.username); text(s.value,2,row.password_hash); text(s.value,3,row.role); sqlite3_bind_int(s.value,4,row.active);
    return step(database_,s.value);
}
Result<void> UserRepository::remove(const std::string& username) {
    Statement s; if (!prepare(database_, "DELETE FROM users WHERE username=?;", s)) return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    text(s.value,1,username); return step(database_,s.value);
}
Result<UserRow> UserRepository::find(const std::string& username) const {
    Statement s; if (!prepare(const_cast<Database&>(database_), "SELECT username,password_hash,role,active FROM users WHERE username=?;", s)) return Result<UserRow>::failure(ErrorCode::INTERNAL_ERROR);
    text(s.value,1,username); if (sqlite3_step(s.value)!=SQLITE_ROW) return Result<UserRow>::failure(ErrorCode::NOT_FOUND);
    return Result<UserRow>::success({reinterpret_cast<const char*>(sqlite3_column_text(s.value,0)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,1)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,2)),sqlite3_column_int(s.value,3)!=0});
}
Result<std::vector<UserRow>> UserRepository::all() const {
    Statement s; if (!prepare(const_cast<Database&>(database_), "SELECT username,password_hash,role,active FROM users ORDER BY username;", s)) return Result<std::vector<UserRow>>::failure(ErrorCode::INTERNAL_ERROR);
    std::vector<UserRow> rows; while (sqlite3_step(s.value)==SQLITE_ROW) rows.push_back({reinterpret_cast<const char*>(sqlite3_column_text(s.value,0)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,1)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,2)),sqlite3_column_int(s.value,3)!=0}); return Result<std::vector<UserRow>>::success(rows);
}

Result<void> StudentRepository::save(const StudentRow& r) {
    Statement s; if (!prepare(database_, "INSERT INTO student_profiles(srn,username,name,date_of_birth,contact,programme,semester,active) VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(srn) DO UPDATE SET username=excluded.username,name=excluded.name,date_of_birth=excluded.date_of_birth,contact=excluded.contact,programme=excluded.programme,semester=excluded.semester,active=excluded.active;",s)) return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
    text(s.value,1,r.srn);text(s.value,2,r.username);text(s.value,3,r.name);text(s.value,4,r.date_of_birth);text(s.value,5,r.contact);text(s.value,6,r.programme);sqlite3_bind_int(s.value,7,r.semester);sqlite3_bind_int(s.value,8,r.active);return step(database_,s.value);
}
Result<StudentRow> StudentRepository::find(const std::string& srn) const {
    Statement s;if(!prepare(const_cast<Database&>(database_),"SELECT srn,username,name,date_of_birth,contact,programme,semester,active FROM student_profiles WHERE srn=?;",s))return Result<StudentRow>::failure(ErrorCode::INTERNAL_ERROR);text(s.value,1,srn);if(sqlite3_step(s.value)!=SQLITE_ROW)return Result<StudentRow>::failure(ErrorCode::NOT_FOUND);return Result<StudentRow>::success({reinterpret_cast<const char*>(sqlite3_column_text(s.value,0)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,1)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,2)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,3)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,4)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,5)),sqlite3_column_int(s.value,6),sqlite3_column_int(s.value,7)!=0});
}
Result<std::vector<StudentRow>> StudentRepository::all() const { Statement s;if(!prepare(const_cast<Database&>(database_),"SELECT srn,username,name,date_of_birth,contact,programme,semester,active FROM student_profiles ORDER BY srn;",s))return Result<std::vector<StudentRow>>::failure(ErrorCode::INTERNAL_ERROR);std::vector<StudentRow> rows;while(sqlite3_step(s.value)==SQLITE_ROW)rows.push_back({reinterpret_cast<const char*>(sqlite3_column_text(s.value,0)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,1)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,2)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,3)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,4)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,5)),sqlite3_column_int(s.value,6),sqlite3_column_int(s.value,7)!=0});return Result<std::vector<StudentRow>>::success(rows);}

Result<void> CourseRepository::save(const CourseRow& r){Statement s;if(!prepare(database_,"INSERT INTO courses(code,title,credits,faculty_username) VALUES(?,?,?,?) ON CONFLICT(code) DO UPDATE SET title=excluded.title,credits=excluded.credits,faculty_username=excluded.faculty_username;",s))return Result<void>::failure(ErrorCode::INTERNAL_ERROR);text(s.value,1,r.code);text(s.value,2,r.title);sqlite3_bind_int(s.value,3,r.credits);text(s.value,4,r.faculty_username);return step(database_,s.value);}
Result<CourseRow> CourseRepository::find(const std::string& code) const {Statement s;if(!prepare(const_cast<Database&>(database_),"SELECT code,title,credits,faculty_username FROM courses WHERE code=?;",s))return Result<CourseRow>::failure(ErrorCode::INTERNAL_ERROR);text(s.value,1,code);if(sqlite3_step(s.value)!=SQLITE_ROW)return Result<CourseRow>::failure(ErrorCode::NOT_FOUND);return Result<CourseRow>::success({reinterpret_cast<const char*>(sqlite3_column_text(s.value,0)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,1)),sqlite3_column_int(s.value,2),reinterpret_cast<const char*>(sqlite3_column_text(s.value,3))});}
Result<std::vector<CourseRow>> CourseRepository::all() const {Statement s;if(!prepare(const_cast<Database&>(database_),"SELECT code,title,credits,faculty_username FROM courses ORDER BY code;",s))return Result<std::vector<CourseRow>>::failure(ErrorCode::INTERNAL_ERROR);std::vector<CourseRow> rows;while(sqlite3_step(s.value)==SQLITE_ROW)rows.push_back({reinterpret_cast<const char*>(sqlite3_column_text(s.value,0)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,1)),sqlite3_column_int(s.value,2),reinterpret_cast<const char*>(sqlite3_column_text(s.value,3))});return Result<std::vector<CourseRow>>::success(rows);}

Result<void> EnrollmentRepository::save(const EnrollmentRow& r){Statement s;if(!prepare(database_,"INSERT INTO enrollments(srn,course_code,semester) VALUES(?,?,?);",s))return Result<void>::failure(ErrorCode::INTERNAL_ERROR);text(s.value,1,r.srn);text(s.value,2,r.course_code);sqlite3_bind_int(s.value,3,r.semester);return step(database_,s.value);}
Result<std::vector<EnrollmentRow>> EnrollmentRepository::all() const {Statement s;if(!prepare(const_cast<Database&>(database_),"SELECT srn,course_code,semester FROM enrollments ORDER BY srn,course_code,semester;",s))return Result<std::vector<EnrollmentRow>>::failure(ErrorCode::INTERNAL_ERROR);std::vector<EnrollmentRow> rows;while(sqlite3_step(s.value)==SQLITE_ROW)rows.push_back({reinterpret_cast<const char*>(sqlite3_column_text(s.value,0)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,1)),sqlite3_column_int(s.value,2)});return Result<std::vector<EnrollmentRow>>::success(rows);}

Result<void> GradeRepository::save(const GradeRow& r){Statement s;if(!prepare(database_,"INSERT INTO grades(srn,course_code,semester,has_marks,marks,letter,grade_points) VALUES(?,?,?,?,?,?,?) ON CONFLICT(srn,course_code,semester) DO UPDATE SET has_marks=excluded.has_marks,marks=excluded.marks,letter=excluded.letter,grade_points=excluded.grade_points;",s))return Result<void>::failure(ErrorCode::INTERNAL_ERROR);text(s.value,1,r.srn);text(s.value,2,r.course_code);sqlite3_bind_int(s.value,3,r.semester);sqlite3_bind_int(s.value,4,r.has_marks);sqlite3_bind_double(s.value,5,r.marks);text(s.value,6,r.letter);sqlite3_bind_double(s.value,7,r.grade_points);return step(database_,s.value);}
Result<std::vector<GradeRow>> GradeRepository::all() const {Statement s;if(!prepare(const_cast<Database&>(database_),"SELECT srn,course_code,semester,has_marks,marks,letter,grade_points FROM grades ORDER BY srn,course_code,semester;",s))return Result<std::vector<GradeRow>>::failure(ErrorCode::INTERNAL_ERROR);std::vector<GradeRow> rows;while(sqlite3_step(s.value)==SQLITE_ROW)rows.push_back({reinterpret_cast<const char*>(sqlite3_column_text(s.value,0)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,1)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,5)),sqlite3_column_int(s.value,2),sqlite3_column_int(s.value,3)!=0,sqlite3_column_double(s.value,4),sqlite3_column_double(s.value,6)});return Result<std::vector<GradeRow>>::success(rows);}

Result<void> AttendanceRepository::save(const AttendanceRow& r){Statement s;if(!prepare(database_,"INSERT INTO attendance(srn,course_code,semester,date,session_details,status) VALUES(?,?,?,?,?,?) ON CONFLICT(srn,course_code,semester,date,session_details) DO UPDATE SET status=excluded.status;",s))return Result<void>::failure(ErrorCode::INTERNAL_ERROR);text(s.value,1,r.srn);text(s.value,2,r.course_code);sqlite3_bind_int(s.value,3,r.semester);text(s.value,4,r.date);text(s.value,5,r.session_details);text(s.value,6,r.status);return step(database_,s.value);}
Result<std::vector<AttendanceRow>> AttendanceRepository::all() const {Statement s;if(!prepare(const_cast<Database&>(database_),"SELECT srn,course_code,semester,date,session_details,status FROM attendance ORDER BY srn,course_code,semester,date;",s))return Result<std::vector<AttendanceRow>>::failure(ErrorCode::INTERNAL_ERROR);std::vector<AttendanceRow> rows;while(sqlite3_step(s.value)==SQLITE_ROW)rows.push_back({reinterpret_cast<const char*>(sqlite3_column_text(s.value,0)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,1)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,3)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,4)),reinterpret_cast<const char*>(sqlite3_column_text(s.value,5)),sqlite3_column_int(s.value,2)});return Result<std::vector<AttendanceRow>>::success(rows);}

Result<void> AuditRepository::append(const AuditRow& r){Statement s;if(!prepare(database_,"INSERT INTO audit_records(username,action,entity,entity_id,details) VALUES(?,?,?,?,?);",s))return Result<void>::failure(ErrorCode::INTERNAL_ERROR);text(s.value,1,r.username);text(s.value,2,r.action);text(s.value,3,r.entity);text(s.value,4,r.entity_id);text(s.value,5,r.details);return step(database_,s.value);}

}  // namespace srs::persistence
