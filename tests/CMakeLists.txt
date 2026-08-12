find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_ring_buffer test_ring_buffer.cpp)
target_link_libraries(test_ring_buffer PRIVATE serialkit_headless Qt6::Test)
add_test(NAME test_ring_buffer COMMAND test_ring_buffer)

add_executable(test_frame_strategy test_frame_strategy.cpp)
target_link_libraries(test_frame_strategy PRIVATE serialkit_headless Qt6::Test)
add_test(NAME test_frame_strategy COMMAND test_frame_strategy)

add_executable(test_send_encoding test_send_encoding.cpp)
target_link_libraries(test_send_encoding PRIVATE serialkit_headless Qt6::Test)
add_test(NAME test_send_encoding COMMAND test_send_encoding)
