#include "core/application.h"

#include <string>

int main() {
    return std::string(student_record::application_name()) ==
                   "Student Record System"
               ? 0
               : 1;
}
