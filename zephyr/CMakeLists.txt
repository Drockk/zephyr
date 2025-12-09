add_library(zephyr SHARED)

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
        FILE_SET HEADERS FILES
        "include/zephyr/context/context.hpp"
        "include/zephyr/core/application.hpp"
        "include/zephyr/core/plugin.hpp"
        "include/zephyr/execution/strandOp.hpp"
        "include/zephyr/execution/strandState.hpp"
        "include/zephyr/http/httpMessages.hpp"
        "include/zephyr/io/ioUringContext.hpp"
        "include/zephyr/plugins/dummyPlugin.hpp"
        "include/zephyr/utils/anySender.hpp"
    PRIVATE
        "source/io/ioUringContext.cpp"
)

target_link_libraries(zephyr
    PUBLIC
        STDEXEC::stdexec
        ${LIBURING_LIBRARIES}
)

target_include_directories(zephyr
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBURING_INCLUDE_DIRS}
)
