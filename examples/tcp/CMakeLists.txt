add_executable(example_tcp_server example_tcp_server.cpp)
target_link_libraries(example_tcp_server PRIVATE zephyr::zephyr)

add_executable(example_tcp_client example_tcp_client.cpp)
target_link_libraries(example_tcp_client PRIVATE zephyr::zephyr)
