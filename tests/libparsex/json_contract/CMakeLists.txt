add_executable(json_contract_tests
    envelope_schema_test.cpp
    conventions_test.cpp
    version_test.cpp
    versioning_test.cpp
    validator_smoke_test.cpp
    payload_schemas_test.cpp
    schema_conformance_test.cpp
    envelope_helper_test.cpp
    hardening_contract_test.cpp
    json_contract_e2e_test.cpp
)
target_link_libraries(
    json_contract_tests
    PRIVATE
    libparsex
    GTest::gtest
    GTest::gtest_main
)
target_compile_definitions(
    json_contract_tests
    PRIVATE
    PARSEX_ENVELOPE_SCHEMA_FILE="${CMAKE_SOURCE_DIR}/schemas/envelope.schema.json"
    PARSEX_CONVENTIONS_FILE="${CMAKE_SOURCE_DIR}/schemas/CONVENTIONS.md"
    PARSEX_VERSIONING_FILE="${CMAKE_SOURCE_DIR}/schemas/VERSIONING.md"
    PARSEX_SCHEMAS_DIR="${CMAKE_SOURCE_DIR}/schemas"
)
gtest_discover_tests(json_contract_tests)

add_test(NAME contract_envelope_consistency
    COMMAND json_contract_tests --gtest_filter=EnvelopeConsistencyTest.*)
set_tests_properties(contract_envelope_consistency PROPERTIES LABELS "hardening_contract")

add_test(NAME contract_strict_validate_nesting
    COMMAND json_contract_tests --gtest_filter=SpanNestingTest.*)
set_tests_properties(contract_strict_validate_nesting PROPERTIES LABELS "hardening_contract")

add_test(NAME contract_cli_mcp_version_consistency
    COMMAND json_contract_tests --gtest_filter=VersionConsistencyTest.*)
set_tests_properties(contract_cli_mcp_version_consistency PROPERTIES LABELS "hardening_contract")
