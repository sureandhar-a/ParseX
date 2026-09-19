# Schema data-model tests: CachedSchema + SchemaResolutionResult placeholders.
add_executable(schema_tests
    cached_schema_test.cpp
    xml_schema_raii_test.cpp
)
target_link_libraries(
    schema_tests
    PRIVATE
    libparsex
    GTest::gtest
    GTest::gtest_main
)
# Directory holding the user-supplied .xsd files (see resources/schemas/README.md).
target_compile_definitions(
    schema_tests
    PRIVATE
    PARSEX_SCHEMA_DIR="${CMAKE_SOURCE_DIR}/resources/schemas"
)
gtest_discover_tests(schema_tests)
