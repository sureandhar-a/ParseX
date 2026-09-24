# Executable for the local assistant bridge (stdio only).
add_executable(parsex-mcp main.cpp)

find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(parsex-mcp PRIVATE libparsex nlohmann_json::nlohmann_json)

target_link_libraries(parsex-mcp PRIVATE parsex_sanitizers)

# Live pipe coverage: drive the built binary over a real stdin/stdout pipe.
add_test(NAME transport.pipe_single_roundtrip
    COMMAND bash -c "out=$(printf '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}\n' | \"$0\"); echo \"$out\"; echo \"$out\" | python3 -c 'import json,sys; doc=json.loads(sys.stdin.read()); assert doc[\"jsonrpc\"]==\"2.0\" and doc[\"id\"]==1' && test \"$(printf '%s' \"$out\" | tail -c 1 | xxd -p)\" = \"0a\" && test $(printf '%s' \"$out\" | grep -c '^') -eq 1" $<TARGET_FILE:parsex-mcp>)
set_tests_properties(transport.pipe_single_roundtrip PROPERTIES
    PASS_REGULAR_EXPRESSION "\"jsonrpc\"")

add_test(NAME transport.pipe_survives_garbage
    COMMAND bash -c "out=$(printf 'not json at all\n{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}\n' | \"$0\" 2>/dev/null); echo \"$out\"; echo \"$out\" | python3 -c 'import json,sys; doc=json.loads(sys.stdin.read()); assert doc[\"id\"]==2'" $<TARGET_FILE:parsex-mcp>)
set_tests_properties(transport.pipe_survives_garbage PROPERTIES
    PASS_REGULAR_EXPRESSION "\"id\":2")

add_test(NAME transport.pipe_stdout_pure
    COMMAND bash -c "out=$(printf 'garbage line\n{\"jsonrpc\":\"1.0\",\"id\":3}\n{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/list\"}\n' | \"$0\" 2>/dev/null); echo \"$out\"; echo \"$out\" | python3 -c 'import json,sys; [json.loads(l) for l in sys.stdin.read().strip().splitlines() if l.strip()]; print(\"all lines are json\")'" $<TARGET_FILE:parsex-mcp>)
set_tests_properties(transport.pipe_stdout_pure PROPERTIES
    PASS_REGULAR_EXPRESSION "all lines are json")
