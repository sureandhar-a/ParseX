# Executable for the local assistant bridge (stdio only).
add_executable(parsex-mcp main.cpp)

find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(parsex-mcp PRIVATE libparsex nlohmann_json::nlohmann_json)

target_link_libraries(parsex-mcp PRIVATE parsex_sanitizers)
