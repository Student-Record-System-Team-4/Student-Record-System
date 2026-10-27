cmake_minimum_required(VERSION 3.20)

project(StudentRecordSystemCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(FetchContent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

add_library(srs_core INTERFACE)

target_include_directories(srs_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_library(password_hasher
    src/core/password_hasher.cpp
)

target_link_libraries(password_hasher PUBLIC
    srs_core
    PkgConfig::SODIUM
)

add_executable(test_password_hasher
    tests/test_password_hasher.cpp
)

target_link_libraries(test_password_hasher PRIVATE
    password_hasher
    Catch2::Catch2WithMain
)

catch_discover_tests(test_password_hasher)

add_executable(test_validator
    tests/test_validator.cpp
)

target_link_libraries(test_validator PRIVATE
    srs_core
    Catch2::Catch2WithMain
)

catch_discover_tests(test_validator)