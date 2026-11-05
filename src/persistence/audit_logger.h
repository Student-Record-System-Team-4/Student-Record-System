#ifndef STUDENT_RECORD_SYSTEM_PERSISTENCE_AUDIT_LOGGER_H
#define STUDENT_RECORD_SYSTEM_PERSISTENCE_AUDIT_LOGGER_H

#include <string>

#include "core/result.h"
#include "persistence/database.h"

namespace srs::persistence {

class AuditLogger {
public:
    explicit AuditLogger(Database& database);

    srs::core::Result<void> log(
        const std::string& username,
        const std::string& action,
        const std::string& entity,
        const std::string& entity_id,
        const std::string& details = ""
    );

private:
    Database& database_;
};

}  // namespace srs::persistence

#endif  // STUDENT_RECORD_SYSTEM_PERSISTENCE_AUDIT_LOGGER_H
