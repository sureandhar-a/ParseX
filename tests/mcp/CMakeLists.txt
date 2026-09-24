add_executable(mcp_tests
    transport_test.cpp
    protocol_test.cpp
    discover_test.cpp
    registry_test.cpp
    schemas_test.cpp
)

find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(
    mcp_tests
    PRIVATE
    libparsex
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(mcp_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/parsex-mcp
)

include(GoogleTest)
gtest_discover_tests(mcp_tests)
