#include "course/course_service.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "persistence/repositories.h"

namespace srs::course {

using srs::auth::Permission;
using srs::auth::Role;
using srs::core::ErrorCode;
using srs::core::Result;

CourseService::CourseService(
    srs::auth::UserAccountService& accounts,
    srs::auth::AuthenticationService& authentication,
    srs::student::StudentProfileService& students)
    : accounts_(accounts),
      authentication_(authentication),
      students_(students) {}

CourseService::CourseService(
    srs::auth::UserAccountService& accounts,
    srs::auth::AuthenticationService& authentication,
    srs::student::StudentProfileService& students,
    srs::persistence::Database& database)
    : CourseService(accounts, authentication, students) {

    database_ = &database;

    srs::persistence::CourseRepository course_repository(database);
    const auto courses = course_repository.all();

    if (courses) {
        for (const auto& course : *courses.value()) {
            courses_[course.code] = {
                course.code,
                course.title,
                course.credits,
                course.faculty_username,
                course.active
            };
        }
    }

    srs::persistence::EnrollmentRepository enrollment_repository(database);
    const auto enrollments = enrollment_repository.all();

    if (enrollments) {
        for (const auto& enrollment : *enrollments.value()) {
            enrollments_.push_back({
                enrollment.srn,
                enrollment.course_code,
                enrollment.semester
            });
        }
    }
}

bool CourseService::valid_course(const Course& course) {
    if (course.code.size() < 2 || course.code.size() > 16) {
        return false;
    }

    for (char ch : course.code) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-') {
            return false;
        }
    }

    if (course.title.empty() || course.title.size() > 100) {
        return false;
    }

    bool has_visible_character = false;

    for (char ch : course.title) {
        const auto value = static_cast<unsigned char>(ch);

        if (value < 32 || value == 127) {
            return false;
        }

        if (ch != ' ') {
            has_visible_character = true;
        }
    }

    return has_visible_character &&
           course.credits >= MIN_CREDITS &&
           course.credits <= MAX_CREDITS;
}

bool CourseService::valid_faculty(const std::string& username) const {
    const auto role = authentication_.role_of(username);
    return role && *role.value() == Role::Faculty;
}

Result<srs::student::StudentProfile> CourseService::find_active_student(
    const std::string& session_id,
    const std::string& srn) const {

    using srs::student::StudentProfile;

    if (srn.empty()) {
        return Result<StudentProfile>::failure(ErrorCode::INVALID_INPUT);
    }

    srs::student::StudentSearchQuery query;
    query.srn = srn;

    const auto matches = students_.search_students(session_id, query);

    if (!matches) {
        return Result<StudentProfile>::failure(ErrorCode::INTERNAL_ERROR);
    }

    for (const auto& profile : *matches.value()) {
        if (profile.srn == srn) {
            return Result<StudentProfile>::success(profile);
        }
    }

    return Result<StudentProfile>::failure(ErrorCode::NOT_FOUND);
}

Result<void> CourseService::create_course(
    const std::string& session_id,
    const Course& course) {

    const auto allowed =
        accounts_.authorize(session_id, Permission::ManageCourses);

    if (!allowed) {
        return Result<void>::failure(allowed.error());
    }

    if (!valid_course(course) || !valid_faculty(course.faculty_username)) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    if (courses_.count(course.code) > 0) {
        return Result<void>::failure(ErrorCode::ALREADY_EXISTS);
    }

    if (database_) {
        srs::persistence::CourseRepository repository(*database_);

        if (!repository.save({
                course.code,
                course.title,
                course.credits,
                course.faculty_username,
                course.active
            })) {
            return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
        }
    }

    courses_[course.code] = course;

    return Result<void>::success();
}

Result<void> CourseService::update_course(
    const std::string& session_id,
    const Course& course) {

    const auto allowed =
        accounts_.authorize(session_id, Permission::ManageCourses);

    if (!allowed) {
        return Result<void>::failure(allowed.error());
    }

    if (!valid_course(course) || !valid_faculty(course.faculty_username)) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    const auto existing = courses_.find(course.code);

    if (existing == courses_.end()) {
        return Result<void>::failure(ErrorCode::NOT_FOUND);
    }

    if (database_) {
        srs::persistence::CourseRepository repository(*database_);

        if (!repository.save({
                course.code,
                course.title,
                course.credits,
                course.faculty_username,
                course.active
            })) {
            return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
        }
    }

    existing->second = course;

    return Result<void>::success();
}

Result<void> CourseService::deactivate_course(
    const std::string& session_id,
    const std::string& course_code) {

    const auto allowed =
        accounts_.authorize(session_id, Permission::ManageCourses);

    if (!allowed) {
        return Result<void>::failure(allowed.error());
    }

    const auto existing = courses_.find(course_code);

    if (existing == courses_.end()) {
        return Result<void>::failure(ErrorCode::NOT_FOUND);
    }

    if (!existing->second.active) {
        return Result<void>::success();
    }

    if (database_) {
        srs::persistence::CourseRepository repository(*database_);
        const auto result = repository.deactivate(course_code);

        if (!result) {
            return Result<void>::failure(result.error());
        }
    }

    existing->second.active = false;
    return Result<void>::success();
}

Result<std::vector<Course>> CourseService::list_courses(
    const std::string& session_id) const {

    using CourseList = std::vector<Course>;

    std::vector<Course> result;

    if (accounts_.authorize(session_id, Permission::ManageCourses)) {
        for (const auto& entry : courses_) {
            result.push_back(entry.second);
        }

        return Result<CourseList>::success(std::move(result));
    }

    const auto faculty_access =
        accounts_.authorize(session_id, Permission::ViewCourseRoster);

    if (!faculty_access) {
        return Result<CourseList>::failure(faculty_access.error());
    }

    const auto username = authentication_.username_for_session(session_id);

    if (!username) {
        return Result<CourseList>::failure(ErrorCode::SESSION_ERROR);
    }

    for (const auto& entry : courses_) {
        if (entry.second.faculty_username == *username.value()) {
            result.push_back(entry.second);
        }
    }

    return Result<CourseList>::success(std::move(result));
}

Result<void> CourseService::enroll_student(
    const std::string& session_id,
    const std::string& srn,
    const std::string& course_code,
    int semester) {

    const auto allowed =
        accounts_.authorize(session_id, Permission::EnrollStudents);

    if (!allowed) {
        return Result<void>::failure(allowed.error());
    }

    if (semester < MIN_SEMESTER || semester > MAX_SEMESTER) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    const auto course = courses_.find(course_code);

    if (course == courses_.end()) {
        return Result<void>::failure(ErrorCode::NOT_FOUND);
    }

    if (!course->second.active) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    const auto student = find_active_student(session_id, srn);

    if (!student) {
        return Result<void>::failure(student.error());
    }

    if (is_enrolled(srn, course_code, semester)) {
        return Result<void>::failure(ErrorCode::ALREADY_EXISTS);
    }

    if (database_) {
        srs::persistence::EnrollmentRepository repository(*database_);

        if (!repository.save({srn, course_code, semester})) {
            return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
        }
    }

    enrollments_.push_back({srn, course_code, semester});

    return Result<void>::success();
}

Result<void> CourseService::remove_student(
    const std::string& session_id,
    const std::string& srn,
    const std::string& course_code,
    int semester) {

    const auto allowed =
        accounts_.authorize(session_id, Permission::EnrollStudents);

    if (!allowed) {
        return Result<void>::failure(allowed.error());
    }

    if (semester < MIN_SEMESTER || semester > MAX_SEMESTER) {
        return Result<void>::failure(ErrorCode::INVALID_INPUT);
    }

    if (courses_.count(course_code) == 0) {
        return Result<void>::failure(ErrorCode::NOT_FOUND);
    }

    const auto enrollment = std::find_if(
        enrollments_.begin(),
        enrollments_.end(),
        [&](const Enrollment& item) {
            return item.srn == srn &&
                   item.course_code == course_code &&
                   item.semester == semester;
        });

    if (enrollment == enrollments_.end()) {
        return Result<void>::failure(ErrorCode::NOT_FOUND);
    }

    if (database_) {
        srs::persistence::EnrollmentRepository repository(*database_);

        if (!repository.remove(srn, course_code, semester)) {
            return Result<void>::failure(ErrorCode::INTERNAL_ERROR);
        }
    }

    enrollments_.erase(enrollment);
    return Result<void>::success();
}

Result<std::vector<RosterEntry>> CourseService::view_roster(
    const std::string& session_id,
    const std::string& course_code,
    int semester) const {

    using Roster = std::vector<RosterEntry>;

    const auto allowed =
        accounts_.authorize(session_id, Permission::ViewCourseRoster);

    if (!allowed) {
        return Result<Roster>::failure(allowed.error());
    }

    const auto course = courses_.find(course_code);

    if (course == courses_.end()) {
        return Result<Roster>::failure(ErrorCode::NOT_FOUND);
    }

    if (semester != 0 &&
        (semester < MIN_SEMESTER || semester > MAX_SEMESTER)) {
        return Result<Roster>::failure(ErrorCode::INVALID_INPUT);
    }

    const auto username = authentication_.username_for_session(session_id);

    if (!username) {
        return Result<Roster>::failure(ErrorCode::SESSION_ERROR);
    }

    const auto role = authentication_.role_of(*username.value());

    if (!role) {
        return Result<Roster>::failure(ErrorCode::SESSION_ERROR);
    }

    if (*role.value() == Role::Faculty &&
        course->second.faculty_username != *username.value()) {
        return Result<Roster>::failure(ErrorCode::PERMISSION_DENIED);
    }

    Roster roster;

    for (const auto& enrollment : enrollments_) {
        if (enrollment.course_code != course_code ||
            (semester != 0 && enrollment.semester != semester)) {
            continue;
        }

        const auto student =
            find_active_student(session_id, enrollment.srn);

        if (!student) {
            continue;
        }

        roster.push_back({
            enrollment.srn,
            student.value()->name,
            enrollment.semester,
            enrollment.course_code
        });
    }

    std::sort(
        roster.begin(),
        roster.end(),
        [](const RosterEntry& a, const RosterEntry& b) {
            return a.srn != b.srn
                       ? a.srn < b.srn
                       : a.semester < b.semester;
        });

    return Result<Roster>::success(std::move(roster));
}

Result<Course> CourseService::get_course(
    const std::string& course_code) const {

    const auto course = courses_.find(course_code);

    if (course == courses_.end()) {
        return Result<Course>::failure(ErrorCode::NOT_FOUND);
    }

    return Result<Course>::success(course->second);
}

bool CourseService::is_enrolled(
    const std::string& srn,
    const std::string& course_code,
    int semester) const {

    for (const auto& enrollment : enrollments_) {
        if (enrollment.srn == srn &&
            enrollment.course_code == course_code &&
            (semester == 0 || enrollment.semester == semester)) {
            return true;
        }
    }

    return false;
}

bool CourseService::is_assigned_faculty(
    const std::string& username,
    const std::string& course_code) const {

    const auto course = courses_.find(course_code);

    return course != courses_.end() &&
           course->second.faculty_username == username;
}

// F-021: Student views only their own course enrollments.
// Admin can view any student's enrollments.
Result<std::vector<RosterEntry>> CourseService::view_student_enrollments(
    const std::string& session_id,
    const std::string& srn) const {

    using Roster = std::vector<RosterEntry>;

    if (srn.empty()) {
        return Result<Roster>::failure(ErrorCode::INVALID_INPUT);
    }

    const auto username =
        authentication_.username_for_session(session_id);

    if (!username) {
        return Result<Roster>::failure(ErrorCode::SESSION_ERROR);
    }

    const auto role =
        authentication_.role_of(*username.value());

    if (!role) {
        return Result<Roster>::failure(role.error());
    }

    std::string student_name;

    if (*role.value() == Role::Student) {
        const auto allowed =
            accounts_.authorize(session_id, Permission::ViewOwnReports);

        if (!allowed) {
            return Result<Roster>::failure(allowed.error());
        }

        const auto profile = students_.view_own_profile(session_id);

        if (!profile) {
            return Result<Roster>::failure(profile.error());
        }

        if (profile.value()->srn != srn) {
            return Result<Roster>::failure(ErrorCode::PERMISSION_DENIED);
        }
        student_name = profile.value()->name;
    } else if (*role.value() == Role::Admin) {
        const auto allowed =
            accounts_.authorize(
                session_id,
                Permission::GenerateInstitutionReports);

        if (!allowed) {
            return Result<Roster>::failure(allowed.error());
        }

        const auto student = find_active_student(session_id, srn);

        if (!student) {
            return Result<Roster>::failure(student.error());
        }
        student_name = student.value()->name;
    } else {
        return Result<Roster>::failure(ErrorCode::PERMISSION_DENIED);
    }

    Roster result;

    for (const auto& enrollment : enrollments_) {
        if (enrollment.srn != srn) {
            continue;
        }

        const auto course = courses_.find(enrollment.course_code);

        if (course == courses_.end()) {
            continue;
        }

        result.push_back({
            srn,
            student_name,
            enrollment.semester,
            enrollment.course_code
        });
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const RosterEntry& a, const RosterEntry& b) {
            if (a.semester != b.semester) {
                return a.semester < b.semester;
            }
            return a.course_code < b.course_code;
        });

    return Result<Roster>::success(std::move(result));
}

}  // namespace srs::course