# Dependencies.cmake - External dependencies management

include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

CPMAddPackage("gh:gabime/spdlog@1.16.0")


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
