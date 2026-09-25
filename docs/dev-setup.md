# Development environment setup

This guide is for contributors. End-user build instructions live in
`README.md`; test-running reference lives in `docs/testing.md`. This file
covers the extra tooling a contributor needs beyond a plain build.

## Sanitizer build

```bash
cmake --preset build-asan
cmake --build build/asan --parallel
ctest --test-dir build/asan --output-on-failure
# macOS (leak detection is unreliable on Apple Silicon):
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/asan --output-on-failure
```

## Fuzzing toolchain

Fuzz harnesses need Clang with the libFuzzer runtime — Homebrew LLVM on
macOS, not the Xcode command-line tools:

```bash
cmake -S . -B build/fuzz -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DPARSEX_ENABLE_FUZZING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/fuzz --target parsex_fuzz_parser --target cli_argv_fuzz --target mcp_stdio_frame_fuzz
```

Seed corpus lives in `fuzz/corpus/parser`. See `docs/testing.md` for run
commands and what to do with a crash.

## Coverage tools

Either `gcovr` (GCC) or `llvm-cov` with `llvm-profdata` (Clang):

```bash
sh scripts/coverage.sh
# HTML: build/coverage/index.html
python3 scripts/check_coverage_floor.py
```

Flags are opt-in via `PARSEX_ENABLE_COVERAGE` and never touch default builds.

## Editor setup

Generate a compilation database for clangd-based editors:

```bash
cmake --preset default -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

The default configure already writes `build/debug/compile_commands.json`;
point the editor at it for code completion and navigation.

Lint configuration lives in `.clang-tidy` at the repo root (checks,
warnings-as-errors list, header filter). There is currently no `.clang-format`
file; match the surrounding code style until one is added (see
`docs/coding-conventions.md`).

## Debugging tips

Run a single check verbosely:

```bash
ctest --test-dir build/debug -R "<test-name-regex>" --output-on-failure -V
```

Attach a debugger to either binary directly — both are ordinary local
processes:

```bash
lldb -- build/debug/parsex-cli/parsex-cli parse --input examples/sample.arxml
# or: gdb --args build/debug/parsex-mcp/parsex-mcp
```

For the stdio server, pipe a request instead of attaching first:

```bash
printf '{"jsonrpc":"2.0","id":1,"method":"tools/list"}\n' | build/debug/parsex-mcp/parsex-mcp
```

Reading a sanitizer report: the failing address line names the access
(read/write, size), the stack below it names the ParseX frame first — fix at
the ParseX frame, not in the standard library frames beneath it. Re-run the
single check above under the sanitizer build to confirm the fix.
