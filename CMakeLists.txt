
cmake_minimum_required(VERSION 3.20)

project(StudentRecordSystemCore LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(FetchContent)

# SQLite
find_package(SQLite3 QUIET)

if(SQLite3_FOUND)
    set(SRS_SQLITE_TARGET SQLite::SQLite3)
else()
    FetchContent_Declare(
        sqlite3
        URL https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(sqlite3)

    add_library(srs_sqlite STATIC
        ${sqlite3_SOURCE_DIR}/sqlite3.c
    )

    target_include_directories(
        srs_sqlite
        PUBLIC ${sqlite3_SOURCE_DIR}
    )

    set(SRS_SQLITE_TARGET srs_sqlite)
endif()

# Catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)


# Core library
add_library(srs_core STATIC
    src/core/application.cpp
    src/auth/access_control.cpp
    src/auth/authentication.cpp
    src/auth/password_hasher.cpp
    src/auth/user_accounts.cpp
    src/student/student_profiles.cpp
    src/course/course_service.cpp
    src/grades/grade_service.cpp
    src/attendance/attendance_service.cpp
    src/reports/report_service.cpp
    src/persistence/database.cpp
    src/persistence/database_backup.cpp
    src/persistence/repositories.cpp
    src/persistence/audit_logger.cpp
    src/ui/cli.cpp
)

target_include_directories(
    srs_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Application executable
add_executable(student_record_system
    src/main.cpp
)

target_link_libraries(
    student_record_system
    PRIVATE srs_core
)

# Test executable
add_executable(test_validator
    tests/test_validator.cpp
    tests/test_access_control.cpp
    tests/test_authentication.cpp
    tests/test_password_hasher.cpp
    tests/test_student_profiles.cpp
    tests/test_user_accounts.cpp
    tests/test_course_service.cpp
    tests/test_course_persistence.cpp
    tests/test_grade_service.cpp
    tests/test_grade_persistence.cpp
    tests/test_attendance_service.cpp
    tests/test_attendance_persistence.cpp
    tests/test_report_service.cpp
    tests/test_cli.cpp
    tests/application_test.cpp
    tests/sample_test.cpp
    tests/test_persistence.cpp
    tests/course_management_test.cpp
    tests/test_database_backup_audit.cpp
)

target_link_libraries(
    test_validator
    PRIVATE
        srs_core
        Catch2::Catch2WithMain
)

# Register tests with CTest
add_test(
    NAME test_validator
    COMMAND test_validator
)

# Smoke test
if(NOT WIN32)
    add_test(
        NAME cli_smoke
        COMMAND sh -c "rm -f cli-smoke-test.db && printf 'admin01\nAdmin12345\nAdmin12345\n1\nadmin01\nAdmin12345\n0\n0\n' | $<TARGET_FILE:student_record_system> cli-smoke-test.db && rm -f cli-smoke-test.db"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    set_tests_properties(cli_smoke PROPERTIES
        PASS_REGULAR_EXPRESSION "Welcome, admin01 \\(Admin\\)\\..*Goodbye\\."
    )
endif()