# add_executable(example_01 example_01.cpp)

# set(CMAKE_CXX_STANDARD 23)

# target_link_libraries(example_01 PRIVATE zephyr)

# add_executable(example_tcp_01 example_tcp_01.cpp)

# set(CMAKE_CXX_STANDARD 23)

# target_link_libraries(example_tcp_01 PRIVATE STDEXEC::stdexec zephyr)

add_executable(example_tcp_02 example_tcp_02.cpp)

set(CMAKE_CXX_STANDARD 23)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)

target_include_directories(example_tcp_02 PRIVATE ${LIBURING_INCLUDE_DIRS})
target_link_libraries(example_tcp_02 PRIVATE zephyr STDEXEC::stdexec ${LIBURING_LIBRARIES})
