cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP OFF CACHE BOOL "Disable C++ modules")
set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "Disable C++ module scanning")

project(zephyr)

add_subdirectory(examples)
add_subdirectory(zephyr)
