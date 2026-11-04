#ifndef SRS_PERSISTENCE_REPOSITORIES_H
#define SRS_PERSISTENCE_REPOSITORIES_H

#include <string>
#include <vector>

#include "database.h"
#include "result.h"

namespace srs::persistence {

struct UserRow {
    std::string username;
    std::string password_hash;
    std::string role;
    bool active = true;
};

struct StudentRow {
    std::string srn;
    std::string name;
    std::string email;
    std::string department;
    int semester = 0;
};

struct CourseRow {
    std::string course_code;
    std::string course_name;
    int credits = 0;
};

struct EnrollmentRow {
    std::string srn;
    std::string course_code;
    int semester = 0;
};

struct GradeRow {
    std::string srn;
    std::string course_code;
    int semester = 0;
    std::string grade;
};

struct AttendanceRow {
    std::string srn;
    std::string course_code;
    std::string date;
    std::string session_details;
    std::string status;
    int semester = 0;
};

struct AuditRow {
    std::string username;
    std::string action;
    std::string details;
    std::string timestamp;
};

class UserRepository {
public:
    explicit UserRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const UserRow& row);
    srs::core::Result<void> remove(const std::string& username);
    srs::core::Result<UserRow> find(const std::string& username);
    srs::core::Result<std::vector<UserRow>> all();

private:
    Database& database_;
};

class StudentRepository {
public:
    explicit StudentRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const StudentRow& row);
    srs::core::Result<std::vector<StudentRow>> all();
    srs::core::Result<StudentRow> find(const std::string& srn);

private:
    Database& database_;
};

class CourseRepository {
public:
    explicit CourseRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const CourseRow& row);
    srs::core::Result<std::vector<CourseRow>> all();
    srs::core::Result<CourseRow> find(const std::string& course_code);
    srs::core::Result<void> deactivate(const std::string& course_code);

private:
    Database& database_;
};

class EnrollmentRepository {
public:
    explicit EnrollmentRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const EnrollmentRow& row);
    srs::core::Result<std::vector<EnrollmentRow>> all();
    srs::core::Result<void> remove(
        const std::string& srn,
        const std::string& course_code,
        int semester
    );

private:
    Database& database_;
};

class GradeRepository {
public:
    explicit GradeRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const GradeRow& row);
    srs::core::Result<std::vector<GradeRow>> all();

private:
    Database& database_;
};

class AttendanceRepository {
public:
    explicit AttendanceRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const AttendanceRow& row);
    srs::core::Result<std::vector<AttendanceRow>> all();

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

#endif  // SRS_PERSISTENCE_REPOSITORIES_H
