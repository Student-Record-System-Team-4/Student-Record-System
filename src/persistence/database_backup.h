#ifndef STUDENT_RECORD_SYSTEM_PERSISTENCE_DATABASE_BACKUP_H
#define STUDENT_RECORD_SYSTEM_PERSISTENCE_DATABASE_BACKUP_H

#include <string>

#include "core/result.h"
#include "persistence/database.h"

namespace srs::persistence {

class DatabaseBackup {
public:
    // Creates a consistent SQLite backup at the given destination path.
    static srs::core::Result<void> create_backup(
        Database& database,
        const std::string& backup_path
    );

    // Restores the database contents from the given backup file.
    static srs::core::Result<void> restore_backup(
        Database& database,
        const std::string& backup_path
    );
};

}  // namespace srs::persistence

#endif  // STUDENT_RECORD_SYSTEM_PERSISTENCE_DATABASE_BACKUP_H
