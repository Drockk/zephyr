cmake_minimum_required(VERSION 3.28)

# Enable clang-scan-deps integration for C++ modules.
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)

project(zephyr)

add_subdirectory(examples)
add_subdirectory(zephyr)
