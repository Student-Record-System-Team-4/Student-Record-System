#ifndef SRS_PERSISTENCE_REPOSITORIES_H
#define SRS_PERSISTENCE_REPOSITORIES_H

#include <string>
#include <vector>

#include "core/result.h"
#include "persistence/database.h"

namespace srs::persistence {

struct UserRow {
    std::string username;
    std::string password_hash;
    std::string role;
    bool active = true;
};

struct StudentRow {
    std::string srn;
    std::string username;
    std::string name;
    std::string date_of_birth;
    std::string phone;
    std::string department;
    int semester = 0;
    bool active = true;
};

struct CourseRow {
    std::string course_code;
    std::string course_name;
    int credits = 0;
    std::string faculty_username;
};

struct EnrollmentRow {
    std::string srn;
    std::string course_code;
    int semester = 0;
};

struct GradeRow {
    std::string srn;
    std::string course_code;
    std::string letter;
    int semester = 0;
    bool has_marks = false;
    double marks = 0.0;
    double grade_points = 0.0;
};

class UserRepository {
public:
    explicit UserRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const UserRow& row);
    srs::core::Result<std::vector<UserRow>> all() const;
    srs::core::Result<UserRow> find(const std::string& username) const;

private:
    Database& database_;
};

class StudentRepository {
public:
    explicit StudentRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const StudentRow& row);
    srs::core::Result<std::vector<StudentRow>> all() const;
    srs::core::Result<StudentRow> find(const std::string& srn) const;

private:
    Database& database_;
};

class CourseRepository {
public:
    explicit CourseRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const CourseRow& row);
    srs::core::Result<std::vector<CourseRow>> all() const;
    srs::core::Result<CourseRow> find(const std::string& course_code) const;

private:
    Database& database_;
};

class EnrollmentRepository {
public:
    explicit EnrollmentRepository(Database& database) : database_(database) {}

    srs::core::Result<void> save(const EnrollmentRow& row);
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

}  // namespace srs::persistence

#endif  // SRS_PERSISTENCE_REPOSITORIES_H
