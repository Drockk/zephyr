# Dependencies.cmake - External dependencies management

include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.17.0
    OPTIONS
        "SPDLOG_BUILD_SHARED ON"
        "SPDLOG_BUILD_EXAMPLES OFF"
        "SPDLOG_BUILD_TESTS OFF"
)

CPMAddPackage("gh:catchorg/Catch2@3.12.0")

# Provide Catch2 CMake module and config so catch_discover_tests is available
if(Catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
endif()
find_package(Catch2 3 CONFIG REQUIRED)

# stdexec - C++ Senders/Receivers implementation
CPMAddPackage(
    NAME stdexec
    GITHUB_REPOSITORY NVIDIA/stdexec
    GIT_TAG main
    OPTIONS
        "STDEXEC_BUILD_EXAMPLES OFF"
)

# liburing - Linux io_uring library
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)
