#ifndef STUDENT_RECORD_SYSTEM_PERSISTENCE_REPOSITORIES_H
#define STUDENT_RECORD_SYSTEM_PERSISTENCE_REPOSITORIES_H

#include <string>
#include <vector>

#include "persistence/database.h"

namespace srs::persistence {

struct UserRow {
    std::string username, password_hash, role;
    bool active = true;
};

struct StudentRow {
    std::string srn, username, name, date_of_birth, contact, programme;
    int semester = 0;
    bool active = true;
};

struct CourseRow {
    std::string code, title;
    int credits = 0;
    std::string faculty_username;
    bool active = true;
};

struct EnrollmentRow {
    std::string srn, course_code;
    int semester = 0;
};

struct GradeRow {
    std::string srn, course_code, letter;
    int semester = 0;
    bool has_marks = false;
    double marks = 0;
    double grade_points = 0;
};

struct AttendanceRow {
    std::string srn, course_code, date, session_details, status;
    int semester = 0;
};

struct AuditRow {
    std::string username, action, entity, entity_id, details;
};

class UserRepository {
public:
    explicit UserRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const UserRow& row);
    srs::core::Result<void> remove(const std::string& username);
    srs::core::Result<UserRow> find(const std::string& username) const;
    srs::core::Result<std::vector<UserRow>> all() const;

private:
    Database& database_;
};

class StudentRepository {
public:
    explicit StudentRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const StudentRow& row);
    srs::core::Result<StudentRow> find(const std::string& srn) const;
    srs::core::Result<std::vector<StudentRow>> all() const;

private:
    Database& database_;
};

class CourseRepository {
public:
    explicit CourseRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const CourseRow& row);
    srs::core::Result<void> deactivate(const std::string& code);
    srs::core::Result<CourseRow> find(const std::string& code) const;
    srs::core::Result<std::vector<CourseRow>> all() const;

private:
    Database& database_;
};

class EnrollmentRepository {
public:
    explicit EnrollmentRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const EnrollmentRow& row);
    srs::core::Result<void> remove(
        const std::string& srn,
        const std::string& course_code,
        int semester);
    srs::core::Result<std::vector<EnrollmentRow>> all() const;

private:
    Database& database_;
};

class GradeRepository {
public:
    explicit GradeRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const GradeRow& row);
    srs::core::Result<std::vector<GradeRow>> all() const;

private:
    Database& database_;
};

class AttendanceRepository {
public:
    explicit AttendanceRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const AttendanceRow& row);
    srs::core::Result<std::vector<AttendanceRow>> all() const;

private:
    Database& database_;
};

class AuditRepository {
public:
    explicit AuditRepository(Database& database) : database_(database) {}

    srs::core::Result<void> append(const AuditRow& row);

private:
    Database& database_;
};

}  // namespace srs::persistence

#endif
