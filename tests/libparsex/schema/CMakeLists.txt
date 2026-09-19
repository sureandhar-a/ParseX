# Schema data-model tests: CachedSchema + SchemaResolutionResult placeholders.
add_executable(schema_tests
    cached_schema_test.cpp
)
target_link_libraries(
    schema_tests
    PRIVATE
    libparsex
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(schema_tests)
