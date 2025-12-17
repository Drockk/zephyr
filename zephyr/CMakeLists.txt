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

list(FILTER ZEPHYR_HEADERS
     EXCLUDE REGEX "/00_[^/]+/"
)

file(GLOB_RECURSE ZEPHYR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
)

list(FILTER ZEPHYR_SOURCES
     EXCLUDE REGEX "/00_[^/]+/"
)


target_sources(zephyr
    PUBLIC
        FILE_SET HEADERS FILES ${ZEPHYR_HEADERS}
    PRIVATE
        ${ZEPHYR_SOURCES}
)

target_link_libraries(zephyr
    PUBLIC
        spdlog
        STDEXEC::stdexec
        ${LIBURING_LIBRARIES}
)

target_include_directories(zephyr
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBURING_INCLUDE_DIRS}
)
