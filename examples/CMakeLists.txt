# =============================================================================
# Zephyr Examples
# =============================================================================

add_executable(example_01_basic example_01_basic.cpp)
target_link_libraries(example_01_basic PRIVATE zephyr::zephyr)

add_executable(example_01_udp example_01_udp.cpp)
target_link_libraries(example_01_udp PRIVATE zephyr::zephyr)

# Example: TCP Server
# add_executable(example_tcp_02 example_tcp_02.cpp)
# target_link_libraries(example_tcp_02 PRIVATE zephyr::zephyr)
