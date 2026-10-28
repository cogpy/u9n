cmake_minimum_required(VERSION 3.14)

# Find Google Test (tests are skipped, not faked, where it is not installed)
find_package(GTest)
if(NOT GTest_FOUND)
    message(WARNING "GoogleTest not found: reservoircpp tests are not built")
    return()
endif()
include(GoogleTest)

# node_test.cpp and model_test.cpp are not built yet: they include
# reservoircpp/node.hpp, which does not compile (duplicate member declarations),
# and link a `reservoircpp` target that no longer exists.

# Deep Tree Echo test
add_executable(deep_tree_echo_test deep_tree_echo_test.cpp)
target_link_libraries(deep_tree_echo_test PRIVATE deep_tree_echo GTest::GTest GTest::Main)
gtest_discover_tests(deep_tree_echo_test)
