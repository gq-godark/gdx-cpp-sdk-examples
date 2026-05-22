cmake_minimum_required(VERSION 3.25)
project(godark_cpp_examples LANGUAGES CXX)

# Resolve godark from the bundled SDK install tree.
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdk")

find_package(godark REQUIRED)

add_subdirectory(examples)
