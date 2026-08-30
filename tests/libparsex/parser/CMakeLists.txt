add_executable(parser_tests dummy_test.cpp)

target_link_libraries(parser_tests PRIVATE GTest::gtest GTest::gtest_main)

gtest_discover_tests(parser_tests)