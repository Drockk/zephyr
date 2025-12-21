file(GLOB_RECURSE UNIT_TESTS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

message(STATUS ${UNIT_TESTS_SOURCES})

# Create a test target per source file with a valid name
set(CATCH_TEST_TARGETS)
foreach(src ${UNIT_TESTS_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE Catch2WithMain zephyr)
  list(APPEND CATCH_TEST_TARGETS ${name})
endforeach()

# Export the collected test targets to the parent scope
set(CATCH_TEST_TARGETS ${CATCH_TEST_TARGETS} PARENT_SCOPE)

# Register tests with CTest via Catch2 discovery
include(Catch)
foreach(test_target ${CATCH_TEST_TARGETS})
  catch_discover_tests(${test_target})
endforeach()
