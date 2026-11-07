# CMake generated Testfile for 
# Source directory: E:/SE Lab/Mini Project/Student-Record-System
# Build directory: E:/SE Lab/Mini Project/Student-Record-System/build-coverage
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("test_validator" "E:/SE Lab/Mini Project/Student-Record-System/build-coverage/test_validator.exe")
set_tests_properties("test_validator" PROPERTIES  _CMAKE_TEST_BUILD_DEPENDS "E:/SE Lab/Mini Project/Student-Record-System/build-coverage/test_validator.exe" _BACKTRACE_TRIPLES "E:/SE Lab/Mini Project/Student-Record-System/CMakeLists.txt;127;add_test;E:/SE Lab/Mini Project/Student-Record-System/CMakeLists.txt;0;")
subdirs("_deps/catch2-build")
