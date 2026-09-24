add_executable(mcp_tests
    transport_test.cpp
    protocol_test.cpp
    discover_test.cpp
    registry_test.cpp
    schemas_test.cpp
    tools_list_test.cpp
    dispatch_test.cpp
    parse_tools_test.cpp
    write_tools_test.cpp
    version_test.cpp
    cancel_test.cpp
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

target_compile_definitions(
    mcp_tests
    PRIVATE
    PARSEX_MCP_BINARY="$<TARGET_FILE:parsex-mcp>"
    PARSEX_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)

add_dependencies(mcp_tests parsex-mcp)

include(GoogleTest)
gtest_discover_tests(mcp_tests)
