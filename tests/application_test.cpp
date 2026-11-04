#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/application.h"

TEST_CASE("Application reports its name", "[application]") {
    REQUIRE(std::string(student_record::application_name()) ==
            "Student Record System");
}
