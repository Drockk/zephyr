# CMakeList.txt : CMake project for zephyr, include source and define
# project specific logic here.
#

project (zephyr
    VERSION 0.0.001
    LANGUAGES CXX
)

add_library (zephyr)
add_library (zephyr::zephyr ALIAS zephyr)

target_sources(
    zephyr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source/core/Application.cpp
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/core/Application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/core/Entrypoint.hpp
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET zephyr PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
