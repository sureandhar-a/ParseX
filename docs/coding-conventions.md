# Coding conventions

These are the conventions already used across the codebase, written down so
new code stays consistent without review repetition. When this doc and the
code disagree, the code wins and this doc gets fixed.

## Domain structs: plain aggregates

Domain types (`Cluster`, `EcuInstance`, `Frame`, `Pdu`, `Signal`,
`SignalGroup`, `RawSpan`, `ParsedFile`, `ParsedProject`) are plain
aggregates: public data members, no inheritance for reuse (composition
instead — see `docs/decisions/0001-composition-over-inheritance.md`), no
constructors unless genuinely needed. `CommonFields` is composed in, not
inherited.

Default `operator==` where equality is needed (`RawSpan` is the canonical
example: `bool operator==(const RawSpan&) const = default;`). Cheap
mechanical equality — never hand-rolled member-by-member comparison.

## Headers and namespaces

- Public headers live under `libparsex/include/parsex/<area>/...`;
  implementation files stay under `libparsex/src/` (mirrored per area:
  `diff/`, `validator/`, `write/`, `telemetry/`, `json_contract/`).
- Domain structs and engine facades (`Parser`, `Validator`, `DiffEngine`,
  `WriteEngine`) live in the global namespace and are used unqualified.
- Helpers live under `parsex::<area>`: `parsex::json_contract`
  (`wrapEnvelope`, `kContractVersion`), `parsex::telemetry` (`ScopedSpan`,
  `TelemetryConfig`, `TelemetryContext`), `parsex::cli` (`classify`,
  `colorEnabled`, `ExitCode`).
- The two executables are thin: `parsex-cli/main.cpp` and
  `parsex-mcp/main.cpp` wire engines together and own presentation and
  transport only. No ARXML logic in either `main`.

## Error handling per surface

- `libparsex` internals throw typed exceptions (`ParseError`,
  `ProjectError`, `ReleaseError`, `SchemaResolutionError`,
  `SchemaValidationError`, and friends). No `std::expected` in the public
  headers; callers use try/catch. The parser header documents which failures
  throw vs. which surface as report entries — read it before adding a new
  error path.
- The command line translates every failure through `parsex::cli::classify()`
  into a BSD `sysexits.h` subset behind a single top-level error boundary:
  usage (64), input data (65), I/O (74), internal (70). Findings never change
  the exit code — pass/fail lives in the report body.
- The assistant server translates failures into the two-tier model:
  transport-level JSON-RPC `error` for protocol failures vs. tool-result
  `isError:true` for engine failures (see `parsex-mcp/dispatch.hpp`).
  Callers distinguish "call failed" from "tool ran and reported a problem".

## Formatting

`.clang-format` at the repo root is the enforced formatter (4-space indent,
same-line braces, 120-column limit). `.clang-tidy` next to it lists the
enabled check families. Run clang-tidy on changed files before pushing;
continuous integration enforces both.
