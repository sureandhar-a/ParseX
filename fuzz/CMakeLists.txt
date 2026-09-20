# libFuzzer harness for the Parser (see fuzz_parser.cpp header comment for the
# Clang + libFuzzer build requirements). Deliberately outside CTest: fuzzing
# runs are time-bounded manual/local-CI affairs, not unit tests.
add_executable(fuzz_parser fuzz_parser.cpp)

target_link_libraries(fuzz_parser PRIVATE libparsex)

target_compile_options(fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_options(fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)
