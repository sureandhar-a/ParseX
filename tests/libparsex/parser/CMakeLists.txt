add_executable(parser_tests
    dummy_test.cpp
    secure_reader_test.cpp
    loader_test.cpp
    release_detector_test.cpp
    model_builder_test.cpp
    parsefile_test.cpp
    parseproject_test.cpp
    parse_error_test.cpp
    xxe_test.cpp
    all_fixtures_test.cpp
)

target_link_libraries(
    parser_tests
    PRIVATE
    libparsex
    GTest::gtest
    GTest::gtest_main
)

# Shared fixtures dir (see tests/fixtures/).
target_compile_definitions(
    parser_tests
    PRIVATE
    PARSEX_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)

gtest_discover_tests(parser_tests)