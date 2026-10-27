
cmake_minimum_required(VERSION 3.20)

project(StudentRecordSystemCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

add_library(srs_core STATIC
    src/auth/authentication.cpp
    src/auth/password_hasher.cpp
    src/student/student_profiles.cpp
)

target_include_directories(
    srs_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(test_validator
    tests/test_validator.cpp
    tests/test_authentication.cpp
    tests/test_password_hasher.cpp
    tests/test_student_profiles.cpp
)

target_link_libraries(test_validator PRIVATE
    srs_core
    Catch2::Catch2WithMain
)

catch_discover_tests(test_validator)
