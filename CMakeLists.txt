cmake_minimum_required(VERSION 3.10)
project(ct2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
add_library(ct2lib
    src/board.cpp
    src/uci.cpp
)

target_include_directories(ct2lib PUBLIC src)

add_executable(ct2 src/main.cpp)
target_link_libraries(ct2 PRIVATE ct2lib)

# tests
enable_testing()
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip
)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES tests/*.cpp)
add_executable(ct2_tests ${TEST_SOURCES})
target_link_libraries(ct2_tests PRIVATE ct2lib gtest_main)
add_dependencies(ct2_tests ct2)

add_test(NAME ct2_tests COMMAND ct2_tests)

# Python integration test using Stockfish
add_test(
  NAME full_game_test
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/full_game_test.py
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Reproduce previously illegal position
add_test(
  NAME illegal_move_test
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/illegal_move_test.py
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
