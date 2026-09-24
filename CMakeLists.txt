cmake_minimum_required(VERSION 3.21)

# 1. READ VCPKG_ROOT FROM YOUR SYSTEM ENVIRONMENT
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(ParseX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MINGW)
    # Statically link the MinGW runtime so binaries don't depend on finding
    # the matching libstdc++/libgcc/libwinpthread DLLs on PATH at run time.
    add_link_options(-static-libgcc -static-libstdc++ -static)
endif()
option(PARSEX_ENABLE_CLANG_TIDY "Run clang-tidy on every compile (slow; CI covers this with a dedicated job)" OFF)
find_program(CLANG_TIDY_EXE NAMES clang-tidy PATHS "$ENV{HOMEBREW_PREFIX}/opt/llvm/bin")
if(PARSEX_ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

include(CTest)
# Enable testing in the root so CTest can find all tests
enable_testing()

add_subdirectory(libparsex)
add_subdirectory(parsex-cli)
add_subdirectory(parsex-mcp)
add_subdirectory(tests)

# libFuzzer harness (off by default: needs Clang with the libFuzzer runtime —
# Homebrew LLVM on macOS, not Xcode CLT; see fuzz/fuzz_parser.cpp).
option(PARSEX_ENABLE_FUZZING "Build the libFuzzer parser harness" OFF)
if(PARSEX_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()

# User-supplied AUTOSAR XSD schemas (see resources/schemas/README.md).
# The .xsd files are copyrighted, gitignored, and provided by each user — the
# repo tracks only the layout. Whatever *.xsd files are present locally ship
# alongside the binaries via `cmake --install`, and are also copied into the
# build tree so local runs and tests can find them.
# NOTE: file(COPY) runs at configure time, so re-run configure after adding
# new .xsd files.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/schemas")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/schemas/"
            DESTINATION "share/parsex/schemas"
            FILES_MATCHING PATTERN "*.xsd")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/schemas/"
         DESTINATION "${CMAKE_BINARY_DIR}/resources/schemas"
         FILES_MATCHING PATTERN "*.xsd")
endif()

option(PARSEX_ENABLE_SANITIZERS "Build with ASan+UBSan instrumentation" OFF)

option(PARSEX_ENABLE_COVERAGE "Build with coverage instrumentation (gcov/llvm-cov)" OFF)

option(PARSEX_ENABLE_TELEMETRY "Compile in ParseX's telemetry instrumentation" ON)
if(NOT PARSEX_ENABLE_TELEMETRY)
    add_compile_definitions(PARSEX_TELEMETRY_DISABLED)
endif()

add_library(parsex_sanitizers INTERFACE)
if(PARSEX_ENABLE_SANITIZERS)
  target_compile_options(parsex_sanitizers INTERFACE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all)
  target_link_options(parsex_sanitizers INTERFACE
    -fsanitize=address,undefined)
endif()

target_link_libraries(libparsex PUBLIC parsex_sanitizers)

# Coverage: opt-in only, never on by default. Uses --coverage (GCC/gcov and
# Clang both accept it; Clang pairs with llvm-cov/llvm-profdata for reports,
# GCC with gcovr). Applied to the core library; tools link it transitively.
add_library(parsex_coverage INTERFACE)
if(PARSEX_ENABLE_COVERAGE)
  target_compile_options(parsex_coverage INTERFACE --coverage -O0 -g)
  target_link_options(parsex_coverage INTERFACE --coverage)
endif()
target_link_libraries(libparsex PUBLIC parsex_coverage)

# Coverage report: runs the suite (excluding slow + fuzz loops, which distort
# line numbers) then produces terminal + HTML output under build/coverage.
# Toolchain: llvm-cov/llvm-profdata for Clang, gcovr for GCC.
# One-command local run: scripts/coverage.sh
find_program(LLVM_COV_EXE NAMES llvm-cov PATHS "$ENV{HOMEBREW_PREFIX}/opt/llvm/bin" /opt/homebrew/opt/llvm/bin)
find_program(GCOVR_EXE NAMES gcovr)
add_custom_target(coverage
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -E "hardening_slow|hardening_fuzz|fuzz_parser_smoke|cli_argv_fuzz_smoke|mcp_stdio_frame_fuzz_smoke" || true
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
  COMMAND sh -c "if command -v gcovr >/dev/null 2>&1; then gcovr -r ${CMAKE_SOURCE_DIR} --html --html-details -o ${CMAKE_BINARY_DIR}/coverage/index.html --print-summary; echo 'HTML: ${CMAKE_BINARY_DIR}/coverage/index.html'; elif [ -n '${LLVM_COV_EXE}' ]; then echo 'llvm-cov report: run scripts/coverage.sh for full HTML (needs profdata merge)'; ${LLVM_COV_EXE} gcov --help >/dev/null 2>&1 && echo 'llvm-cov available'; else echo 'No coverage tool found (install gcovr or llvm-cov)'; fi"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Run tests (excluding slow/fuzz) and render coverage to build/coverage/index.html"
  VERBATIM)