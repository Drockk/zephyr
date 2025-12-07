add_executable(example_01 example_01.cpp)

set(CMAKE_CXX_STANDARD 23)

target_link_libraries(example_01 PRIVATE zephyr)
