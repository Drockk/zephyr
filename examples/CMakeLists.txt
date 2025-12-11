# =============================================================================
# Zephyr Examples
# =============================================================================

# Example: TCP Server
add_executable(example_tcp_02 example_tcp_02.cpp)
target_link_libraries(example_tcp_02 PRIVATE zephyr::zephyr)
