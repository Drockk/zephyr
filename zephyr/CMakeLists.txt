add_library(zephyr)

include(../cmake/CPM.cmake)

CPMAddPackage(
  NAME stdexec
  GITHUB_REPOSITORY NVIDIA/stdexec
  GIT_TAG main # This will always pull the latest code from the `main` branch. You may also use a specific release version or tag
)

# Find liburing
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)

target_sources(zephyr
    PUBLIC
        FILE_SET CXX_MODULES FILES
        core/application.cppm
        core/plugin.cppm
        io/ioUringAcceptSender.cppm
        io/ioUringContext.cppm
        plugins/dummyPlugin.cppm
        zephyr.cppm
)

target_link_libraries(zephyr
    STDEXEC::stdexec
    ${LIBURING_LIBRARIES}
)

target_include_directories(zephyr
    PUBLIC
        ${LIBURING_INCLUDE_DIRS}
)
