#include "core/application.h"
#include "persistence/database.h"

#include <iostream>

int main(int argc, char** argv) {
    const std::string database_path =
        argc > 1 ? argv[1] : "student-record-system.db";
    srs::persistence::Database database(database_path);
    if (!database.is_open()) {
        std::cerr << "Unable to initialize database '" << database_path
                  << "': " << database.last_error() << '\n';
        return 1;
    }
    std::cout << student_record::application_name() << '\n';
    return 0;
}
