# libFuzzer harnesses for the Parser. fuzz_parser.cpp is the legacy
# temp-file harness; parser_fuzz_target.cpp is the direct-bytes harness
# used by the automated smoke pass. Deliberately outside the default CTest
# run: fuzzing runs are time-bounded manual/local-CI affairs, not unit tests.
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
