# libFuzzer harnesses for the Parser. fuzz_parser.cpp is the legacy
# temp-file harness; parser_fuzz_target.cpp is the direct-bytes harness
# used by the automated smoke pass.
add_executable(fuzz_parser fuzz_parser.cpp)

target_link_libraries(fuzz_parser PRIVATE libparsex)

target_compile_options(fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_options(fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)

# Thin main-replacement linking the same production library, built only when
# PARSEX_ENABLE_FUZZING is on. Feeds fuzzer bytes directly to the in-memory
# entry point with no filesystem use.
add_executable(parsex_fuzz_parser parser_fuzz_target.cpp)

target_link_libraries(parsex_fuzz_parser PRIVATE libparsex)

target_compile_options(parsex_fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_options(parsex_fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)

include(CTest)
# Short CI-friendly smoke pass (30s) vs the longer manual exploratory run
# (minutes, see fuzz/corpus/parser/README.md): this test proves the harness
# stays green on every fuzz-enabled build without slowing normal builds —
# the whole fuzz/ tree only configures when PARSEX_ENABLE_FUZZING=ON, so a
# default build never pays for it. The scratch output dir comes first so
# mutants never pollute the committed seed corpus; the seed dir second.
# PASS_REGULAR_EXPRESSION guards against an empty no-op run: libFuzzer must
# report a non-zero "Done N runs" count, otherwise the test fails even on
# exit 0.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz-smoke-output")
add_test(
    NAME fuzz_parser_smoke
    COMMAND parsex_fuzz_parser
        "${CMAKE_BINARY_DIR}/fuzz-smoke-output"
        "${CMAKE_SOURCE_DIR}/fuzz/corpus/parser"
        -max_total_time=30
        -max_len=65536
)
set_tests_properties(fuzz_parser_smoke PROPERTIES
    TIMEOUT 120
    LABELS "hardening_fuzz"
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    PASS_REGULAR_EXPRESSION "Done [1-9][0-9]* runs"
)
