cmake_minimum_required(VERSION 3.16)

include("../extra_dirs_common.cmake")

project(test_ui_inputs)

set(EXTRACT_PATTERNS_test_ui_inputs "*/ui_inputs/src/*.cpp" "*/ui_inputs/include/*.hpp")
set(REMOVE_PATTERNS_test_ui_inputs "*/ui_inputs/include/interfaces/*.hpp")

include("../coverage_common.cmake")
setup_gtest_coverage(test_ui_inputs test_ui_inputs.elf)
