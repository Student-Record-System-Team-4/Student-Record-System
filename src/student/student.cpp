
#include "student.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace student_record {
namespace {

std::string normalize(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

bool contains_case_insensitive(
    const std::string& value,
    const std::string& query) {
    return normalize(value).find(normalize(query)) != std::string::npos;
}

bool has_non_whitespace(const std::string& value) {
    return std::any_of(value.begin(), value.end(),
        [](unsigned char ch) {
            return !std::isspace(ch);
        });
}

}  // namespace

bool StudentManager::valid_student(const Student& student) {
    return has_non_whitespace(student.name) &&
           has_non_whitespace(student.srn) &&
           has_non_whitespace(student.date_of_birth) &&
           has_non_whitespace(student.contact) &&
           has_non_whitespace(student.programme) &&
           student.semester >= 1 &&
           student.semester <= 12;
}

bool StudentManager::register_student(const Student& student) {
    if (!valid_student(student)) {
        return false;
    }

    const auto existing = std::find_if(
        students_.begin(), students_.end(),
        [&student](const Student& current) {
            return current.srn == student.srn;
        });

    if (existing != students_.end()) {
        return false;
    }

    students_.push_back(student);
    return true;
}

bool StudentManager::update_student(
    const std::string& srn,
    const Student& updated) {
    if (!valid_student(updated) || updated.srn != srn) {
        return false;
    }

    const auto it = std::find_if(
        students_.begin(), students_.end(),
        [&srn](const Student& student) {
            return student.srn == srn && student.active;
        });

    if (it == students_.end()) {
        return false;
    }

    *it = updated;
    it->active = true;
    return true;
}

bool StudentManager::deactivate_student(const std::string& srn) {
    const auto it = std::find_if(
        students_.begin(), students_.end(),
        [&srn](const Student& student) {
            return student.srn == srn && student.active;
        });

    if (it == students_.end()) {
        return false;
    }

    it->active = false;
    return true;
}

const Student* StudentManager::get_own_profile(
    const std::string& logged_in_srn) const {
    const auto it = std::find_if(
        students_.begin(), students_.end(),
        [&logged_in_srn](const Student& student) {
            return student.srn == logged_in_srn && student.active;
        });

    return it == students_.end() ? nullptr : &(*it);
}

std::vector<Student> StudentManager::search_students(
    const std::string& name,
    const std::string& srn,
    const std::string& programme,
    int semester) const {
    std::vector<Student> results;

    for (const auto& student : students_) {
        if (!student.active) {
            continue;
        }

        if (!name.empty() &&
            !contains_case_insensitive(student.name, name)) {
            continue;
        }

        if (!srn.empty() &&
            !contains_case_insensitive(student.srn, srn)) {
            continue;
        }

        if (!programme.empty() &&
            !contains_case_insensitive(student.programme, programme)) {
            continue;
        }

        if (semester != 0 && student.semester != semester) {
            continue;
        }

        results.push_back(student);
    }

    return results;
}

}  // namespace student_record
