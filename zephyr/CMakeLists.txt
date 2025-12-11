# =============================================================================
# Zephyr Library
# =============================================================================
add_library(zephyr SHARED)
add_library(zephyr::zephyr ALIAS zephyr)

# =============================================================================
# Source Files
# =============================================================================
file(GLOB_RECURSE ZEPHYR_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

file(GLOB_RECURSE ZEPHYR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
)

target_sources(zephyr
    PUBLIC
        FILE_SET HEADERS FILES ${ZEPHYR_HEADERS}
    PRIVATE
        ${ZEPHYR_SOURCES}
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
