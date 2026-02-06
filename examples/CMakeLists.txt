add_subdirectory("tcp")
add_subdirectory("udp")

add_executable(example_udp_client example_udp_client.cpp)
target_compile_features(example_udp_client PRIVATE cxx_std_20)
target_link_libraries(example_udp_client PRIVATE pthread zephyr::zephyr)
