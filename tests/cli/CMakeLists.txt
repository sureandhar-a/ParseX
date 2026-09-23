add_executable(cli_tests
    color_test.cpp
)
target_link_libraries(
    cli_tests
    PRIVATE
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(cli_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/parsex-cli
)

include(GoogleTest)
gtest_discover_tests(cli_tests)
