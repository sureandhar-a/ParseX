# Schema data-model tests: CachedSchema + SchemaResolutionResult placeholders.
add_executable(schema_tests
    cached_schema_test.cpp
    xml_schema_raii_test.cpp
    cache_layout_test.cpp
    atomic_write_test.cpp
    cache_atomicity_test.cpp
    resolve_schema_test.cpp
    schema_missing_test.cpp
    schema_resolution_error_test.cpp
    schema_registry_integration_test.cpp
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
# Shared fixtures dir (see tests/fixtures/).
target_compile_definitions(
    schema_tests
    PRIVATE
    PARSEX_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)
gtest_discover_tests(schema_tests)

add_test(NAME hardening_schema_missing
    COMMAND schema_tests --gtest_filter=SchemaMissingTest.*)
set_tests_properties(hardening_schema_missing PROPERTIES LABELS "hardening_schema")
