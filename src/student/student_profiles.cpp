#include "student/student_profiles.h"

#include "persistence/repositories.h"

namespace srs::student {

StudentProfileService::StudentProfileService(
    srs::auth::AuthenticationService& authentication,
    srs::persistence::Database& database)
    : authentication_(authentication),
      database_(database) {
    srs::persistence::StudentRepository repository(database_);

    auto rows = repository.all();

    if (rows) {
        for (const auto& row : *rows.value()) {
            profiles_.push_back({
                row.username,
                row.name,
                row.srn,
                row.date_of_birth,
                row.phone,
                row.department,
                row.semester,
                row.active
            });
        }
    }
}

}  // namespace srs::student
