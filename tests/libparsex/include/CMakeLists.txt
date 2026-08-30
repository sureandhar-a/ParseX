# Create the test executable for include-level checks.
add_executable(include_tests
    parsex/raw/raw_span_test.cpp # RawSpan unit test source file
    parsex/raw/raw_document_test.cpp # RawDocument unit test
    parsex/raw/raw_node_test.cpp # RawDocument unit test
)
# Link the test binary to the project library and GoogleTest.
target_link_libraries(
    include_tests # executable being linked
    PRIVATE # only this target sees the dependency
    libparsex # ParseX library under test
    GTest::gtest # GoogleTest core framework
    GTest::gtest_main # provides main() for the test binary
)
# Register gtests with CTest so they can be run via ctest.
gtest_discover_tests(include_tests)