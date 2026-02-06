add_executable(example_udp_server example_udp_server.cpp)
target_link_libraries(example_udp_server PRIVATE zephyr::zephyr)

# add_executable(example_tcp_client example_tcp_client.cpp)
# target_link_libraries(example_tcp_client PRIVATE zephyr::zephyr)
