cmake_minimum_required(VERSION 3.28)

project(zephyr
    VERSION 0.1.0
    DESCRIPTION "High-performance async networking framework"
    LANGUAGES CXX
)

# =============================================================================
# Project Options
# =============================================================================
option(ZEPHYR_BUILD_EXAMPLES "Build example applications" ON)
option(ZEPHYR_BUILD_TESTS "Build unit tests" OFF)

# =============================================================================
# Global C++ Settings (fallback if presets not used)
# =============================================================================
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Disable C++ modules (not used in this project)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP OFF CACHE BOOL "Disable C++ modules")
set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "Disable C++ module scanning")

# =============================================================================
# Dependencies
# =============================================================================
include(cmake/Dependencies.cmake)

# =============================================================================
# Subdirectories
# =============================================================================
add_subdirectory(zephyr)

if(ZEPHYR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(ZEPHYR_BUILD_TESTS)
    enable_testing()
    # add_subdirectory(tests)
endif()
