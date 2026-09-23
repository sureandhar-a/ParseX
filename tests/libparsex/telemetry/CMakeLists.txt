add_executable(telemetry_tests
    telemetry_data_model_test.cpp
    scoped_span_test.cpp
    trace_json_test.cpp
)
target_link_libraries(
    telemetry_tests
    PRIVATE
    libparsex
    GTest::gtest
    GTest::gtest_main
)
target_compile_definitions(
    telemetry_tests
    PRIVATE
    PARSEX_SCHEMAS_DIR="${CMAKE_SOURCE_DIR}/schemas"
)
gtest_discover_tests(telemetry_tests)

# Standalone overhead benchmark (PAR-192): not part of CTest, run manually.
add_executable(telemetry_overhead_bench
    telemetry_overhead_bench.cpp
)
target_link_libraries(
    telemetry_overhead_bench
    PRIVATE
    libparsex
)
