add_executable(cli_tests
    color_test.cpp
    json_output_test.cpp
    exit_code_test.cpp
    config_test.cpp
)
find_package(CLI11 CONFIG REQUIRED)
target_link_libraries(
    cli_tests
    PRIVATE
    libparsex
    CLI11::CLI11
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(cli_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/parsex-cli
)
target_compile_definitions(
    cli_tests
    PRIVATE
    PARSEX_CLI_BINARY="$<TARGET_FILE:parsex-cli>"
    PARSEX_SCHEMAS_DIR="${CMAKE_SOURCE_DIR}/schemas"
    PARSEX_FIXTURE_FILE="${CMAKE_SOURCE_DIR}/tests/fixtures/tiny_valid.arxml"
)
# Ensure the CLI binary exists before the tests run (ctest dependency).
add_dependencies(cli_tests parsex-cli)

include(GoogleTest)
gtest_discover_tests(cli_tests)
