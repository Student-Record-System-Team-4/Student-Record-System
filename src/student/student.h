
#ifndef STUDENT_RECORD_SYSTEM_STUDENT_H
#define STUDENT_RECORD_SYSTEM_STUDENT_H

#include <string>
#include <vector>

namespace student_record {

struct Student {
    std::string name;
    std::string srn;
    std::string date_of_birth;
    std::string contact;
    std::string programme;
    int semester = 1;
    bool active = true;
};

class StudentManager {
public:
    bool register_student(const Student& student);
    bool update_student(const std::string& srn, const Student& updated);
    bool deactivate_student(const std::string& srn);

    const Student* get_own_profile(const std::string& logged_in_srn) const;

    std::vector<Student> search_students(
        const std::string& name = "",
        const std::string& srn = "",
        const std::string& programme = "",
        int semester = 0) const;

private:
    std::vector<Student> students_;

    static bool valid_student(const Student& student);
};

}  // namespace student_record

#endif
