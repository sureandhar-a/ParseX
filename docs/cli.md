# ParseX CLI (`parsex`)

`parsex` is the command-line surface around `libparsex` — the first of the Epic's two independently-usable delivery surfaces (the other is `parsex-mcp`). It wraps the Parser, Validator, Diff Engine, and Write Engine behind a stable, scriptable interface with both human-readable and machine-readable (`--json`) output, `sysexits.h`-aligned exit codes, and `flag > env > config > default` configuration precedence.

This document is the single reference for every flag, subcommand, exit code, and config behavior implemented in the CLI Feature (Sprint 10). It is kept in sync with the actual `--help` output — no documented flag without an implementation, no implementation without documentation.

> **Regenerating after a help-text change** — run `parsex --help`, `parsex parse --help`, `parsex validate --help`, `parsex diff --help`, and `parsex write --help` and update the option tables below to match before committing.

## Invocation

```sh
parsex [GLOBAL OPTIONS] <subcommand> [SUBCOMMAND OPTIONS]
```

Exactly one subcommand is required per invocation. With no subcommand:

```
A subcommand is required
Run with --help for more information.
```

## Global options

| Flag | Type | Description | Config key |
|------|------|-------------|------------|
| `-h, --help` | — | Print help and exit (per-subcommand help via `parsex <sub> --help`). | — |
| `-V, --version` | — | Print `libparsexVersion()` (currently `1.0.0`) and exit 0. Works without a subcommand; `parsex parse --version` is not supported (top-level flag only). | — |
| `--json` | flag | Emit machine-readable JSON instead of human-readable text. Must appear **before** the subcommand: `parsex --json parse …` is accepted; `parsex parse --json …` is rejected. When set, stdout is a single JSON envelope per the JSON Output Contract; logs and `--trace` go to stderr. | — |
| `--no-color` | flag | Disable colored output. Human formatters gate any future ANSI codes behind `colorEnabled(--no-color, NO_COLOR, isatty)`. Also disabled when `NO_COLOR` is set in the environment (any value, per [no-color.org](https://no-color.org)) or when stdout is not a TTY. | — |
| `--trace` | flag | Emit a `telemetryReport` JSON payload (kind `telemetryReport`) to **stderr** for the invocation. Each subcommand creates one root `ScopedSpan` (`parsex-cli.<subcommand>`) with child spans from the engines (Parser, Validator, Diff, Write) via `PARSEX_SPAN`. With `--json` the two streams stay separately parseable: stdout is the subcommand's envelope, stderr is the trace. | — |
| `--schema-cache-dir <path>` | `TEXT` | Directory for cached AUTOSAR schemas. The first real config-driven option; see *Configuration precedence* below. | `schema-cache-dir` |
| `--config <path>` | `TEXT` | Read a config file. The file is CLI11's TOML-like format (`key = "value"` per line, reads values as strings). Empty default (not required) — the tool works without a config file. Unknown keys are **silently ignored** (`allow_config_extras` default `Ignore`); a typo like `schema-cach-dir` has no effect and emits no warning (known sharp edge, documented here rather than solved). | — |

### Configuration precedence

For `--schema-cache-dir` (and any future config-driven option added the same way):

```
flag > env var > config file > built-in default
```

* **flag** — an explicit `--schema-cache-dir /path` on the command line.
* **env var** — `PARSEX_SCHEMA_CACHE_DIR` (checked via `getenv`; empty value counts as unset).
* **config file** — `schema-cache-dir = "/path"` inside the file given to `--config`.
* **default** — the XDG-derived cache directory from `getCacheDirectory()`: `$XDG_CACHE_HOME/parsex/schemas/` if set, otherwise `$HOME/.cache/parsex/schemas/`, otherwise the system temp directory. The directory is created if missing.

The implementation logs the effective value and its source at debug level to **stderr** (`[debug] schema-cache-dir=/… (flag|env|config|default)`) so the precedence can be verified without polluting `--json`'s stdout.

CLI11's config file values are applied as if they were CLI arguments, so an explicit flag always beats a config entry; the env-var check is manually slotted between them by testing `argv` for `--schema-cache-dir` before falling back to `getenv`.

### Config file format

CLI11's built-in `set_config()` with a TOML-like reader (not full TOML):

```toml
# parsex.toml — example
schema-cache-dir = "/tmp/my-parsex-cache"
# unknown keys are silently ignored
```

Invoke with:

```sh
parsex --config parsex.toml parse --input file.arxml
```

To test precedence with all layers set:

```sh
PARSEX_SCHEMA_CACHE_DIR=/tmp/from-env parsex --schema-cache-dir /tmp/from-flag --config parsex.toml parse --input file.arxml
# effective: /tmp/from-flag (flag wins)
```

### Environment

* `PARSEX_SCHEMA_CACHE_DIR` — see above.
* `NO_COLOR` — when set (any value), disables colored output.
* `XDG_CACHE_HOME`, `HOME` — used only for the default cache directory when no override is given.

## Subcommands

### `parse` — Parse an ARXML file and report its structure

```
parsex parse --input <file>
parsex --json parse --input <file>
parsex parse --help
```

* **Options**

| Flag | Required | Description |
|------|----------|-------------|
| `-i, --input <file>` | yes | Input ARXML file. Validated at parse time via `CLI::ExistingFile` — a missing path fails fast with a CLI11-formatted error naming the flag and bad value (further clig.dev tone polish belongs to exit-code handling). |

* **Behavior** — calls `Parser::parseFile()` (the Parser and Validator are independent per `lld.md`; the CLI sequences multi-step behavior). On success:

  * Human (default): element counts and release:

    ```
    Parsed tests/fixtures/schema_valid.arxml
      release: 4.2.2
      clusters: 0
      ecuInstances: 0
      frames: 0
      pdus: 0
      signals: 0
      signalGroups: 0
    ```

  * JSON (`--json`): `wrapEnvelope("parseReport", {sourcePath, autosarRelease, counts{clusters, ecuInstances, frames, pdus, signals, signalGroups}, warnings[]})` — validated as `kind: parseReport` against the envelope schema (new kinds added in this Feature; `parseReport` and `writeResult` extend `envelope.schema.json`'s `kind` enum without a strict payload schema).

  On failure (unreadable, malformed XML, unsupported release `autosar_4_0_0.xsd` etc.), the exception propagates to the top-level boundary and the process exits with the mapped `ExitCode` (see below).

### `validate` — Validate an ARXML file against ParseX rules

```
parsex validate --input <file> [--strict]
parsex --json validate --input <file> [--strict]
```

* **Options**

| Flag | Required | Description |
|------|----------|-------------|
| `-i, --input <file>` | yes | Input ARXML file (`CLI::ExistingFile`). |
| `--strict` | no | Strict mode: warnings fail the overall verdict. Without `--strict`, only errors fail; with `--strict`, any warning also makes `overallPassed` false. Individual `severity` labels are never rewritten — strictness only flips the verdict. |

* **Sequencing** — `validate --strict` runs the Parser first and treats any parse failure as a validation finding; without `--strict` the same path is followed but `overallPassed(..., Lenient)` tolerates warnings. This is where the "validate --strict, then parse" note from `lld.md` lives.

* **Exit-code convention** — a `validate` run that finds real validation findings **exits 0**. The pass/fail signal lives in the report body (`payload.passed` in JSON, `Validation passed/failed (N issues)` in human), not in the process exit code. Scripts should check `payload.passed`, not `$?`. Only malformed invocation or I/O (handled by the top-level catch) causes non-zero.

* **Human (default)** — `Validation passed (0 issues)` or `Validation failed (N issues)` plus per-issue lines:

  ```
  Validation failed (1 issues)
    [error] schema.invalid: Element '{http://autosar.org/schema/r4.0}AR-PACKAGE': Missing child element(s). Expected is …
  ```

* **JSON (`--json`)** — `Validator::validateAll(...)->toJson()` envelope with `kind: validationResult`, `payload: {passed, errors[]}`, but with `payload.passed` overridden to `overallPassed(result, Strict/Lenient)` so `--strict` is reflected.

* **Examples**

  ```sh
  # lenient (default) — valid file passes
  parsex validate --input tests/fixtures/schema_valid.arxml
  # Validation passed (0 issues)

  # invalid file fails but exits 0 (check payload.passed)
  parsex validate --input tests/fixtures/schema_invalid.arxml
  # Validation failed (1 issues) …

  # --json variant (parseable)
  parsex --json validate --input tests/fixtures/schema_valid.arxml
  # {"$schema":"…/envelope.json","contractVersion":"1.0.0","toolVersion":"1.0.0","kind":"validationResult","payload":{"errors":[],"passed":true}}

  # strict: warnings-only file passes lenient, fails strict (demonstrated by unit test StrictModeTest)
  ```

### `diff` — Diff two ARXML files

```
parsex diff --base <file> --target <file>
parsex --json diff --base <file> --target <file>
```

* **Options**

| Flag | Required | Description |
|------|----------|-------------|
| `--base <file>` | yes | Base ARXML file (`CLI::ExistingFile`). |
| `--target <file>` | yes | Target ARXML file (`CLI::ExistingFile`). |

* **Behavior** — parses both files via `Parser::parseFile()`, then `DiffEngine::diff(oldProject, newProject)` (path-based matching per domain type, secondary-key move detection, field-level diffing). Human: `DiffReport::toText()` grouped output, or `No differences` when empty. JSON: `DiffReport::toJson()` envelope with `kind: diffReport`, `payload: {entries[], diagnostics[]}`.

* **Examples**

  ```sh
  parsex diff --base tests/fixtures/schema_valid.arxml --target tests/fixtures/schema_valid.arxml
  # No differences

  parsex --json diff --base tests/fixtures/schema_valid.arxml --target tests/fixtures/system-4.2.arxml
  # {"kind":"diffReport","payload":{"entries":[{"kind":"added","elementType":"Frame","newPath":"/AlarmStatus", …}]…}}
  ```

### `write` — Apply a safe edit to an ARXML file

```
parsex write --input <file> --output <file> [--apply]
parsex --json write --input <file> --output <file> [--apply]
```

* **Options**

| Flag | Required | Description |
|------|----------|-------------|
| `-i, --input <file>` | yes | Input ARXML file (`CLI::ExistingFile`). |
| `-o, --output <file>` | yes | Output ARXML file (may not exist; parent directory must be writable when `--apply` is used). |
| `--apply` | no | Actually write the file. **Without this flag the command is a dry-run** — it validates and reports what would be written but never mutates the filesystem. This safety default prevents an accidental `parsex write` from overwriting a file on first use. Documented here and in `parsex write --help`. |

* **Behavior** — parses `input` via `Parser`, then:

  * Dry-run (no `--apply`): calls `WriteEngine::validate(project)` and reports `success = !hasErrors()`. Human: `Dry-run: would write /tmp/out.arxml from … (use --apply to apply)` plus ` (would fail: …)` when `success` is false. JSON: `wrapEnvelope("writeResult", {input, output, applied:false, success})`.

  * Apply (`--apply`): calls `WriteEngine::write(project, output)` — tree construction (correct AUTOSAR namespace/root), deterministic ordering, pretty-printed file write via temp-path + atomic rename. On success human: `Wrote /tmp/out.arxml from …`. On failure (missing SHORT-NAME, unwritable path) the engine throws `WriteError` or `runtime_error("failed to write…")`; the top-level maps it to `DataErr` or `IoErr` and prints `parsex: …` on stderr. JSON on success: `wrapEnvelope("writeResult", {input, output, applied:true, success:true})`.

* **Examples**

  ```sh
  # dry-run (default)
  parsex write --input tests/fixtures/schema_valid.arxml --output /tmp/out.arxml
  # Dry-run: would write /tmp/out.arxml from tests/fixtures/schema_valid.arxml (use --apply to apply)

  # apply
  parsex write --input tests/fixtures/schema_valid.arxml --output /tmp/out.arxml --apply
  # Wrote /tmp/out.arxml from tests/fixtures/schema_valid.arxml

  # --json dry-run (machine-readable)
  parsex --json write --input tests/fixtures/schema_valid.arxml --output /tmp/out.arxml
  # {"kind":"writeResult","payload":{"applied":false,"input":"…","output":"/tmp/out.arxml","success":true},…}
  ```

## Exit codes

Adapted from BSD `sysexits.h` — a small, well-known subset rather than a bespoke scheme so scripts get conventional, greppable statuses. Other `sysexits.h` values have no CLI-observable trigger and are deliberately omitted.

| Code | Name | Value | When |
|------|------|-------|------|
| `Ok` | `EX_OK` | `0` | Success. For `validate`, success means "the tool ran" even if it found findings — check `payload.passed` / human summary instead of `$?`. |
| `Usage` | `EX_USAGE` | `64` | Bad CLI invocation: unknown flag, missing required option, unknown subcommand (`CLI::ParseError` → `Usage`). Example: `parsex --bad-flag` → 64 (mapped via `classify()`). CLI11's own exit codes (105-109) are covered by the `Classify.CliParseErrorMapsToUsage` unit test. |
| `DataErr` | `EX_DATAERR` | `65` | The ARXML input itself is invalid: `ParseError(Syntax)`, `UnsupportedReleaseError`, `DanglingFileReferenceError`, `SchemaResolutionError(Corrupt/Unsupported)`, `WriteError`. Example: `parsex parse --input malformed_unclosed.arxml` → 65 (`parsex: ParseError[Syntax] …`). |
| `IoErr` | `EX_IOERR` | `74` | A file could not be read or written: `ParseError(Io)`, `SchemaResolutionError(SchemaFileMissing)`, `filesystem_error`, `ios_base::failure`, or `runtime_error("failed to write…")` / `runtime_error("failed to move…")` from the Write path. Example: `parsex write … --output /nonexistent_dir/out.arxml --apply` → 74. |
| `Software` | `EX_SOFTWARE` | `70` | Unexpected internal error — anything else is a bug and should be rare. |

The mapping lives in `parsex-cli/exit_code.hpp` (`ExitCode` enum + `classify(const std::exception&)`) and is unit-tested in `tests/cli/exit_code_test.cpp` (one case per category) plus CTest `cli.exit.*` checks for exit-code + stderr substring. Error output always goes to **stderr**, never stdout, even under `--json` (a JSON error envelope on failure is a documented future enhancement, not this Story).

## Telemetry (`--trace`)

When `--trace` is given, the CLI enables telemetry (`TelemetryConfig::setEnabled(true)`) and wraps each subcommand in one root `ScopedSpan("parsex-cli.<subcommand>")`. Child spans from the engines (`parser.*`, `validator.*`, `writeEngine.*`, `diffEngine.*`) nest automatically via thread-local parent tracking. After the subcommand finishes (or after an exception is caught for tracing), the CLI dumps the `telemetryReport` envelope (`Trace::toJson()`) to **stderr**. This keeps `--json`'s stdout clean:

```sh
parsex --json parse --input tests/fixtures/schema_valid.arxml 1> report.json 2> trace.json
# report.json → parseReport envelope
# trace.json  → [debug] line + telemetryReport envelope (filter with: sed -n '/^{/p')
```

Without `--trace`, telemetry is disabled and `PARSEX_SPAN` costs only an `isEnabled()` branch (zero overhead when disabled; `PARSEX_TELEMETRY_DISABLED` compiles it out entirely).

## Worked examples (each subcommand, both modes)

All use `tests/fixtures/schema_valid.arxml` (release `4.2.2`, 0 issues) and `/tmp/out.arxml` for write.

```sh
# parse — human
parsex parse --input tests/fixtures/schema_valid.arxml
# Parsed tests/fixtures/schema_valid.arxml
#   release: 4.2.2
#   clusters: 0 …

# parse — json
parsex --json parse --input tests/fixtures/schema_valid.arxml
# {"kind":"parseReport","payload":{"autosarRelease":"4.2.2","counts":{…},…}}

# validate — human
parsex validate --input tests/fixtures/schema_valid.arxml
# Validation passed (0 issues)

# validate — json
parsex --json validate --input tests/fixtures/schema_valid.arxml
# {"kind":"validationResult","payload":{"errors":[],"passed":true},…}

# diff — human (identical files)
parsex diff --base tests/fixtures/schema_valid.arxml --target tests/fixtures/schema_valid.arxml
# No differences

# diff — json
parsex --json diff --base tests/fixtures/schema_valid.arxml --target tests/fixtures/schema_valid.arxml
# {"kind":"diffReport","payload":{"entries":[],"diagnostics":[]},…}

# write — dry-run human
parsex write --input tests/fixtures/schema_valid.arxml --output /tmp/out.arxml
# Dry-run: would write /tmp/out.arxml from tests/fixtures/schema_valid.arxml (use --apply to apply)

# write — apply json
parsex --json write --input tests/fixtures/schema_valid.arxml --output /tmp/out.arxml --apply
# {"kind":"writeResult","payload":{"applied":true,"input":"…","output":"/tmp/out.arxml","success":true},…}
```

## Testing

* **Help/usage smoke** — `ctest -R cli\.` (bare invocation requires subcommand, each subcommand's `--help`, `--version`).
* **Dual-mode** — `ctest -R cli\.` plus `tests/cli/json_output_test.cpp` (`validatesAgainstSchema` on the envelope).
* **Exit codes** — `ctest -R cli\.exit` plus `Classify.*` unit tests.
* **Config precedence** — `ctest -R cli\.precedence` (`flag > env > config > default` via `ENVIRONMENT`).
* **Golden-file** — `ctest -R ApprovalTests` via `ApprovalTests.cpp` (header-only, vcpkg `approval-tests-cpp`). Human and `--json` are both approved for all four subcommands; `.approved.txt` files live in `tests/cli/` and are reviewed before commit. To regenerate after an intentional output change: run `ctest -R ApprovalTests`, review the `.received.txt` diff, then `cp …/approval_test.*.received.txt …/approval_test.*.approved.txt`.
* **Sanitizers** — `cmake --preset build-asan && cmake --build build/asan && ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/asan -R "cli|ApprovalTests|Classify|CliJsonOutput|ConfigExtras|ColorEnabled"` (the `parsex-cli` target links `parsex_sanitizers` explicitly).

## Cross-links

* The v1 design references in `docs/spec.md`, `docs/hld.md`, and `docs/lld.md` (requirements, high-level and low-level design, all 7 core components) mention the CLI only abstractly — this file (`docs/cli.md`) is the concrete reference those docs should link to for the implemented interface.
* The JSON Output Contract's envelope and schemas live in `schemas/envelope.schema.json` and `schemas/*.schema.json`; the CLI reuses `wrapEnvelope()` and never hardcodes `contractVersion`/`toolVersion`.
* The telemetry model and `TELEMETRY_SCOPE.md` ("no phone-home") are the authority for `--trace` and `ScopedSpan` naming.

## See also

* CLI11: https://github.com/CLIUtils/CLI11 and https://cliutils.github.io/CLI11/book-config.html (`set_config()`)
* Command Line Interface Guidelines: https://clig.dev/
* `sysexits.h`: https://man.archlinux.org/man/sysexits.h.3head.en
* ApprovalTests.cpp: https://github.com/approvals/ApprovalTests.cpp
