# Model defaults checklist: one place listing every field default in the module.
add_executable(model_tests
    defaults_test.cpp # consolidated Parser data-model defaults (cf. docs/lld.md)
)
target_link_libraries(
    model_tests
    PRIVATE
    libparsex
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(model_tests)
