cmake_minimum_required(VERSION 3.20)

project(StudentRecordSystem
    VERSION 0.1.0
    DESCRIPTION "Student Record System"
    LANGUAGES CXX
)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(student_record_core
    src/core/application.cpp
)

target_include_directories(student_record_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(student_record_system
    src/main.cpp
)

target_link_libraries(student_record_system
    PRIVATE
        student_record_core
)

if(BUILD_TESTING)
    add_executable(student_record_application_test
        tests/application_test.cpp
    )

    target_link_libraries(student_record_application_test
        PRIVATE
            student_record_core
    )

    add_test(
        NAME student_record_application_test
        COMMAND student_record_application_test
    )

    add_test(
        NAME student_record_system_smoke_test
        COMMAND student_record_system
    )
endif()
