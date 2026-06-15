add_executable(test_fiber test_fiber.cpp)
target_link_libraries(test_fiber PRIVATE zero)
add_test(NAME test_fiber COMMAND test_fiber)
