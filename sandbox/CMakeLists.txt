project (sandbox
    VERSION 0.0.001
    LANGUAGES CXX
)

add_executable (sandbox)

target_sources(
    sandbox
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source/SandboxApplication.cpp
)

target_link_libraries(
    sandbox
    PRIVATE
        zephyr::zephyr
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET sandbox PROPERTY CXX_STANDARD 20)
endif()
