# Testing guide

How to run each kind of check, why it exists, and where it runs in automation.
Start here instead of hunting through individual areas.

## Unit tests (per-component, default run)

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
# or: ctest --test-dir build/debug --output-on-failure
```

Covers parsing, validation, comparison, writing, shared output shape, timing,
command-line, and assistant-transport behavior. Pipeline stage: first (fastest,
fails fast).

## Sanitizer builds (memory safety)

```bash
cmake --preset build-asan
cmake --build build/asan --parallel
ctest --test-dir build/asan --output-on-failure
# macOS: ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/asan --output-on-failure
```

Catches address and undefined-behavior issues under realistic inputs.
Pipeline stage: after unit tests.

## Fuzz testing (untrusted bytes in)

Targets (built only with fuzzing enabled, needs Clang with the runtime —
Homebrew LLVM on macOS, not Xcode tools):

- parsing entry from raw bytes
- command-line argument parsing
- assistant-transport frame parsing

```bash
cmake -S . -B build/fuzz -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DPARSEX_ENABLE_FUZZING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/fuzz --target parsex_fuzz_parser --target cli_argv_fuzz --target mcp_stdio_frame_fuzz
mkdir -p /tmp/fuzzwork && ASAN_OPTIONS=detect_leaks=0 \
  build/fuzz/fuzz/parsex_fuzz_parser /tmp/fuzzwork fuzz/corpus/parser \
  -max_len=65536 -max_total_time=180
```

Seeds live in `fuzz/corpus/parser` (tiny reused fixtures; see its README).
If a crash appears: minimize it, fix at the source, keep the minimized input
as a regression. Short smoke version (30s, checks for a real run count):

```bash
ctest --test-dir build/fuzz -L hardening_fuzz --output-on-failure
```

Pipeline: nightly/manual only (needs the special compiler; keeps PRs fast).

## Stability and comparison checks

- No-op stability: parse, write with no edits, reparse — must be identical.
- Randomized stability: seeded generator builds varied valid inputs (100 by
  default); failures print the seed for exact rerun.
- Comparison convergence: writing the target side must eliminate the
  comparison result.

```bash
ctest --test-dir build/debug -R "RoundTrip|Reconverge" --output-on-failure
# larger local sweep: PARSEX_ROUNDTRIP_SEEDS=1000 ctest -R RoundTripRandom
```

Pipeline stage: unit-test stage (no special flags needed).

## Shared-output hardening

All report kinds share one envelope (versions, naming, status values); timing
spans from combined runs must nest correctly; both outer surfaces must report
identical versions.

```bash
ctest --test-dir build/debug -L hardening_contract --output-on-failure
ctest --test-dir build/debug -L hardening_schema --output-on-failure
```

Pipeline stage: dedicated step after unit tests, before sanitizers.

## Slow resource checks (nightly only)

Oversized input must fail gracefully within bounds — never hang or crash.
Labeled `hardening_slow`, excluded from default PR runs.

Deeply nested transport input is not a slow check: `parsex-mcp` rejects any
line nested deeper than 128 levels before parsing it, and
`transport.pipe_survives_hostile_input` (plus the `DepthGuard` and
`IdRecovery` unit tests) prove the server answers with an error and keeps
serving. Those run on every PR.

```bash
ctest --test-dir build/debug -L hardening_slow --output-on-failure
```

Pipeline: nightly/manual only.

## Coverage (floor-guarded)

```bash
sh scripts/coverage.sh
# HTML: build/coverage/index.html
python3 scripts/check_coverage_floor.py
```

Needs `gcovr` (GCC) or `llvm-cov`/`llvm-profdata` (Clang). Flags are fully
opt-in via `PARSEX_ENABLE_COVERAGE` and never touch default builds. The floor
(35% initial, set below the observed level to avoid an instantly-failing gate)
is enforced in the pipeline after the coverage target. To raise it: measure
with the script above, update the constant plus this doc, open a PR. Never
exclude files to game the number without justification.

## Adding a new test

- Focused debugging group? Use an existing label: `hardening_fuzz` (fuzz
  smoke), `hardening_contract` (shared output), `hardening_schema`
  (setup + data types), `hardening_slow` (slow/resource, nightly only).
- New input files belong in `tests/fixtures/` with a `hardening_` prefix and
  a comment explaining the edge probed, plus a paired `.expected.txt` where
  the outcome matters.
- New checks run automatically via the existing test binaries — confirm with
  `ctest -N` and `ctest -L <label>` locally before pushing.
