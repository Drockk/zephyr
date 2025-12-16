# Dependencies.cmake - External dependencies management

include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.16.0
    OPTIONS
        "SPDLOG_BUILD_SHARED ON"
        "SPDLOG_BUILD_EXAMPLES OFF"
        "SPDLOG_BUILD_TESTS OFF"
)


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
