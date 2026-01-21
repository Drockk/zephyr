add_subdirectory("tcp")

# UDP examples - standalone, no external dependencies
add_executable(example_udp_server example_udp_server.cpp)
target_compile_features(example_udp_server PRIVATE cxx_std_20)
target_link_libraries(example_udp_server PRIVATE pthread zephyr::zephyr)

add_executable(example_udp_client example_udp_client.cpp)
target_compile_features(example_udp_client PRIVATE cxx_std_20)
target_link_libraries(example_udp_client PRIVATE pthread zephyr::zephyr)
