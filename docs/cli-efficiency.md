# CLI — what you get, and the proof

For anyone evaluating ParseX: every command, what it returns, the test
results that prove it works, and what makes this CLI different on
memory, time, and usability. No token counts here — those live in
[docs/token-efficiency.md](token-efficiency.md) under their own method;
this file is tested differently and never ranked against anything.

## What each command gives you

| command | you get |
|---|---|
| `parse --input F` | release + 6 domain counts + warnings, one screen |
| `--json parse` | same as a `parseReport` envelope for scripts |
| `validate --input F` | `passed/failed (N issues)` + one line per finding |
| `--json validate` | same as a `validationResult` envelope |
| `write --input F --output O` | dry-run verdict, touches nothing without `--apply` |
| `diff --base A --target B` | grouped change list, or `No differences` |

## How each command is tested

Shell-transcript testing: run the real binary, check exit code, stdout,
and stderr against the contract in [docs/cli.md](cli.md). Failure paths
are assertions, not omissions — an unsupported release must exit 65
with `UnsupportedReleaseError` on stderr, a bad flag must exit 64.

## Test evidence (measured 2026-09-25, rev `7f0b97c`, binaries `0.1.0`)

Full suite: **395/395 ctest pass** (`ctest --preset default`, 24.28 s).
CLI-scoped slices, each re-run in isolation:

| area | tests | result |
|---|---|---|
| CLI integration: help, version, JSON/human output, exit codes, precedence (`ctest -R "^cli"`) | 17 | 17/17 pass |
| Golden transcripts, 4 subcommands × human/`--json` (`ctest -R ApprovalTests`) | 8 | 8/8 pass |
| Exit-code mapping + JSON contract (Classify and CliJsonOutput suites) | 15 | 15/15 pass |
| CLI↔MCP version consistency | 1 | 1/1 pass |
| MCP approval sessions (bridge shares the engines) | 6 | 6/6 pass |

Per-file command outcomes (exit codes + verdicts, not tokens):

| file | parse | validate |
|---|---|---|
| `schema_valid.arxml` (327 B) | 4.2.2, zeros, exit 0 | passed (0 issues), exit 0 |
| `parsefile_complete.arxml` (2,590 B) | real counts, exit 0 | failed (1 issue), exit 0 |
| `tiny_valid.arxml` (3,780 B) | `UnsupportedReleaseError`, exit 65 | failed (1 issue: parse.failed), exit 0 |
| `system-4.2.arxml` (71,059 B) | 4.4.0, 1/3/8/7/32/2, exit 0 | failed (87 issues), exit 0 |
| canmatrix 736 KB, third-party ([canmatrix](https://github.com/ebroecker/canmatrix), BSD-2-Clause) | 4.3.0, 1/1/5/13/1162/0, exit 0 | failed (513 issues), exit 0 |
| Win_Ctrl 974 KB, third-party ([AutoToolMD](https://github.com/hnu-esnl/AutoToolMD)) | 4.2.2, zeros (SWC content, out of modeled scope), exit 0 | passed (654 warnings), exit 0 |
| IntLamp_ComCnvRx 479 KB (AutoToolMD) | 4.2.2, zeros, exit 0 | passed (321 warnings), exit 0 |
| `EcuExtract.arxml` 57 KB, third-party (AutoToolMD, release 4.1.1, unsupported) | `UnsupportedReleaseError`, exit 65 | failed (1 issue: parse.failed), exit 0 |
| `diff schema_valid → system-4.2` | — | 49 entries, exit 0 |

`validate` exits 0 even with findings by design — the verdict lives in
the report body (`payload.passed`), not in `$?`; only broken invocation
or I/O is non-zero. Third-party files are fetched, never vendored
(canmatrix BSD-2-Clause; AutoToolMD has no license file — numbers and
URLs only, no redistribution).

## Why this CLI is different

Measured on macOS arm64 with the release build (`cmake --preset
release`, 2026-09-25) via `/usr/bin/time -l` (max RSS, steady state;
first cold run ~0.6 s page-cache, then ~0.01 s).

### Memory: single-digit megabytes

| file | `parse` wall | max RSS |
|---|---|---|
| `system-4.2.arxml` (71 KB) | ~0.02 s | ~3.2 MB |
| canmatrix 736 KB | 0.68 s | ~7.2 MB |
| Win_Ctrl 974 KB | 0.67 s | ~8.6 MB |

`validate` holds findings in memory, so it costs more (~60 MB RSS with
513 findings on the canmatrix file) — parse stays at 3–9 MB on files up
to ~1 MB. No interpreter, no heap-of-heap: one native process.

### Time: release build, same machine

`parse` runs at ~1 MB/s-per-core in release (0.68 s for 736 KB);
`validate` on the same file takes 0.78 s. Debug builds are ~18× slower
(12.15 s) — always measure release.

### Head-to-head: cantools vs ParseX on ARXML files

Same machine (macOS arm64), same files, both timed with
`/usr/bin/time -l` on 2026-09-25. Versions: cantools 44.1.0 installed
via pip; ParseX `0.1.0` release build.

| file | cantools 44.1.0 | ParseX release |
|---|---|---|
| `system-4.2.arxml` (71 KB) | 0.13 s, 37.8 MB RSS, 8 messages / 22 signals | 0.02 s, 3.2 MB RSS, full domain counts — **~6× faster, ~12× less memory** |
| canmatrix 736 KB | refuses: `UnsupportedDatabaseFormatError` | 0.68 s, 7.2 MB RSS, 5 frames / 13 PDUs / 1,162 signals |

Version limits, both directions: cantools' ARXML reader targets system
templates and rejects files outside them (the canmatrix row above is its
own test file failing on the current release). ParseX likewise accepts
only releases 4.2.2–R21-11 and rejects the 4.0.x–4.2.1-era real files
with `UnsupportedReleaseError` (see the per-file table) — release
coverage, not speed, is each tool's boundary, and both are stated
instead of smoothed over.

Memory differences: cantools carries a ~38 MB interpreter-plus-object
baseline before reading a byte; ParseX `parse` scales with the file at
roughly 10× the input size in RSS (3.2 MB for 71 KB → 8.6 MB for
974 KB) because the domain model, not a generic DOM, is all that is
kept. `validate` is the exception on both sides: retained findings
dominate (~60 MB at 513 findings here).

Scopes differ — cantools is a DBC-centric conversion toolkit, ParseX is
an AUTOSAR-semantic one — so read this as existence proof on shared
ground (system-description ARXML), not a blanket ranking. Repro:
`python3 -m pip install cantools==44.1.0`, then
`cantools.database.load_file()` against the sizes above.

### Usability: safe by default, scriptable by design

- **Dry-run by default.** `write` previews without touching disk unless
  `--apply` is passed — an accidental invocation overwrites nothing.
- **Exit codes with no ambiguity.** `sysexits.h` subset (0/64/65/74/70),
  one classifier behind one top-level boundary, and `validate` findings
  never masquerade as process failure.
- **Two outputs, one engine.** Human text for terminals, `--json`
  envelopes for scripts (`--json` goes before the subcommand; stdout
  stays pure JSON, logs go to stderr).
- **Precedence you can audit.** `flag > env > config > default` for
  every config-driven option, with the effective value and its source
  logged to stderr.
- **One static binary.** Release `parsex-cli` is 2.7 MB linking only
  system libraries (no interpreter, venv, or JVM to install); startup
  is ~0.01 s, so per-file shell loops stay cheap.

### Other factors

- **Deterministic.** Same input → byte-identical output, pinned by 8
  golden approval transcripts; `--trace` stderr telemetry never
  pollutes `--json` stdout.
- **Offline and local-only.** No network calls, no phone-home; schemas
  are user-supplied files resolved to a local cache.
- **Safe writes.** Validation before persisting, temp-file + atomic
  rename on apply, fail-fast refusal of degenerate content instead of
  emitting invalid ARXML.
- **Redistribution-safe design.** Official AUTOSAR XSDs are downloaded
  onto the user's machine (hash-verified), never shipped — the repo
  carries the layout and the script, not the copyrighted files.

## Reproducing

```bash
cmake --preset default && cmake --build --preset default
ctest --preset default                                  # 395/395
ctest --test-dir build/debug -R "^cli"                  # CLI integration slice
ctest --test-dir build/debug -R "ApprovalTests"         # golden transcripts
cmake --preset release && cmake --build --preset release
/usr/bin/time -l ./build/release/parsex-cli/parsex-cli parse --input <file>
```
