#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>

#include "persistence/audit_logger.h"
#include "persistence/database.h"
#include "persistence/database_backup.h"

using namespace srs::persistence;

namespace {

const std::string test_database_path = "test_backup_audit.db";
const std::string backup_database_path = "test_backup_audit_backup.db";

void remove_test_files() {
    std::remove(test_database_path.c_str());
    std::remove(backup_database_path.c_str());
}

}  // namespace

TEST_CASE("Database backup creates a backup file") {
    remove_test_files();

    Database database(test_database_path);
    REQUIRE(database.is_open());

    REQUIRE(database.execute(
        "INSERT INTO roles(name) VALUES ('BackupTestRole');"
    ));

    const auto result = DatabaseBackup::create_backup(
        database,
        backup_database_path
    );

    REQUIRE(result.has_value());

    remove_test_files();
}

TEST_CASE("Database restore restores backed up data") {
    remove_test_files();

    {
        Database database(test_database_path);
        REQUIRE(database.is_open());

        REQUIRE(database.execute(
            "INSERT INTO roles(name) VALUES ('RestoreTestRole');"
        ));

        REQUIRE(DatabaseBackup::create_backup(
            database,
            backup_database_path
        ).has_value());
    }

    {
        Database database(test_database_path);
        REQUIRE(database.is_open());

        REQUIRE(database.execute(
            "DELETE FROM roles WHERE name = 'RestoreTestRole';"
        ));

        REQUIRE(DatabaseBackup::restore_backup(
            database,
            backup_database_path
        ).has_value());

        REQUIRE(database.execute(
            "SELECT name FROM roles WHERE name = 'RestoreTestRole';"
        ));
    }

    remove_test_files();
}

TEST_CASE("Audit logger persists audit records") {
    remove_test_files();

    Database database(test_database_path);
    REQUIRE(database.is_open());

    AuditLogger logger(database);

    REQUIRE(logger.log(
        "",
        "CREATE",
        "test_entity",
        "test-001",
        "Created for unit test"
    ).has_value());

    REQUIRE(database.execute(
        "SELECT action FROM audit_records "
        "WHERE action = 'CREATE' AND entity_id = 'test-001';"
    ));

    remove_test_files();
}
