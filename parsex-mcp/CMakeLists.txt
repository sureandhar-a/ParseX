# Executable for the local assistant bridge (stdio only).
add_executable(parsex-mcp main.cpp)

find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(parsex-mcp PRIVATE libparsex nlohmann_json::nlohmann_json)

# Sanitizer build of the executable target. libparsex already links the
# sanitizer interface, but the executable links it explicitly so the
# instrumentation propagates from a dedicated build graph and the bridge
# meets the same bar as the library and the command-line surface.
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

add_test(NAME protocol.pipe_rejects_unknown_version
    COMMAND bash -c "out=$(printf '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2099-01-01\"}}\n' | \"$0\" 2>/dev/null); echo \"$out\"; echo \"$out\" | python3 -c 'import json,sys; doc=json.loads(sys.stdin.read()); assert doc[\"error\"][\"code\"]==-32600 and \"Unsupported protocol version\" in doc[\"error\"][\"message\"]'" $<TARGET_FILE:parsex-mcp>)
set_tests_properties(protocol.pipe_rejects_unknown_version PROPERTIES
    PASS_REGULAR_EXPRESSION "Unsupported protocol version")

add_test(NAME protocol.pipe_rejects_legacy_initialize
    COMMAND bash -c "out=$(printf '{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"initialize\",\"params\":{}}\n' | \"$0\" 2>/dev/null); echo \"$out\"; echo \"$out\" | python3 -c 'import json,sys; doc=json.loads(sys.stdin.read()); assert doc[\"error\"][\"code\"]==-32600 and \"Legacy initialize\" in doc[\"error\"][\"message\"]'" $<TARGET_FILE:parsex-mcp>)
set_tests_properties(protocol.pipe_rejects_legacy_initialize PROPERTIES
    PASS_REGULAR_EXPRESSION "Legacy initialize")

add_test(NAME protocol.pipe_cancel_after_complete
    COMMAND bash -c "out=$(printf '{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/list\"}\n{\"jsonrpc\":\"2.0\",\"method\":\"notifications/cancelled\",\"params\":{\"requestId\":3}}\n' | \"$0\" 2>/dev/null); echo \"$out\"; echo \"$out\" | python3 -c 'import json,sys; docs=[json.loads(l) for l in sys.stdin.read().strip().splitlines()]; assert len(docs)==1 and docs[0][\"id\"]==3'" $<TARGET_FILE:parsex-mcp>)
set_tests_properties(protocol.pipe_cancel_after_complete PROPERTIES
    PASS_REGULAR_EXPRESSION "\"id\":3")
