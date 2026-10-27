
cmake_minimum_required(VERSION 3.20)

project(StudentRecordSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

add_library(password_hasher
    src/auth/password_hasher.cpp
)

target_include_directories(password_hasher
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(password_hasher
    PUBLIC PkgConfig::SODIUM
)

enable_testing()

add_executable(sample_test
    tests/sample_test.cpp
)

target_link_libraries(sample_test
    PRIVATE Catch2::Catch2WithMain
)

add_executable(test_password_hasher
    tests/test_password_hasher.cpp
)

target_link_libraries(test_password_hasher
    PRIVATE password_hasher Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(Catch)

catch_discover_tests(sample_test)
catch_discover_tests(test_password_hasher)
