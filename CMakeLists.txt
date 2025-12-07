cmake_minimum_required(VERSION 3.28)

# Enable clang-scan-deps integration for C++ modules.
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)

project(zephyr)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(examples)
add_subdirectory(zephyr)
