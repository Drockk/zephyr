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
option(ZEPHYR_BUILD_TESTS "Build unit tests" ON)
option(ZEPHYR_ENABLE_CLANG_TIDY "Enable clang-tidy checks" OFF)
option(ZEPHYR_ENABLE_IWYU "Enable include-what-you-use checks" OFF)

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
# Static Analysis Tools
# =============================================================================
if(ZEPHYR_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy requested but not found")
    endif()
endif()

if(ZEPHYR_ENABLE_IWYU)
    find_program(IWYU_EXE NAMES "include-what-you-use" "iwyu" DOC "Path to include-what-you-use executable")
    if(IWYU_EXE)
        message(STATUS "include-what-you-use found: ${IWYU_EXE}")
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXE}")
    else()
        message(WARNING "include-what-you-use requested but not found")
    endif()
endif()

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
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
