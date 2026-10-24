
cmake_minimum_required(VERSION 3.16)

project(StudentRecordSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

add_library(srs_core INTERFACE)
target_include_directories(srs_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_library(password_hasher
    src/auth/password_hasher.cpp
)

target_include_directories(password_hasher PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(password_hasher
    PUBLIC
        srs_core
        PkgConfig::SODIUM
)

add_executable(test_password_hasher
    tests/test_password_hasher.cpp
)

target_link_libraries(test_password_hasher
    PRIVATE password_hasher
)

enable_testing()
add_test(NAME password_hasher_tests COMMAND test_password_hasher)
