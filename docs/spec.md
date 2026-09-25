# ParseX — Spec

Last updated: 2026-09-25
Status: reconciled against as-built system (see status callouts per section)

> Reconciliation note: sections below carry Implemented / Partial / Planned
> status against the shipped Parser, Validator, Diff Engine, Write Engine,
> Schema Registry, JSON Output Contract, Telemetry, command line, and
> assistant server. Where the original draft named a different command, tool,
> or file, the as-built name is given inline. Claims with no shipped
> counterpart are kept and marked Planned rather than deleted.
>
> Document map: this file is what/why (requirements and scope). `hld.md` is
> components and data flow. `lld.md` is header-verified interfaces plus
> original per-component design. Put new requirements here, new structure in
> `hld.md`, new interfaces in `lld.md`.

## Problem

No existing ARXML tool exposes structured, agent-safe access to AUTOSAR data — that's the gap ParseX targets. v1 focuses on the CAN communication stack on AUTOSAR Classic Platform as the proving ground for round-trip fidelity, validation, semantic diff, and AI-agent-native (MCP) access, per REQUIREMENTS.md.

## Scope decisions locked for v1

- **Protocol slice (FR-6):** full CAN stack — clusters, ECU instances, frames, PDUs, signals, signal groups. Not LIN/FlexRay/Ethernet yet; the JSON envelope is designed to extend to them later (see Feature: JSON Output Contract) but no non-CAN implementation ships in v1.
- **AUTOSAR baseline (FR-9):** 4.2.2 through R21-11, auto-detected per file. A file outside that range is a hard "unsupported release" error, not a best-effort lossy parse.
- **Performance target (NFR-5):** single ECU extract, 1–10 MB, interactive-use latency.
- **MCP client target:** generic/spec-compliant clients — no client-specific certification for v1.
- **Write-mode toggle (FR-14):** Status: Partial — as built, there is no server startup flag. The assistant `write_arxml` tool dry-runs by default and only writes when explicitly asked to apply, with the preview in the response. The original draft's `parsex mcp --allow-write` startup flag was not implemented.
- **Write-back policy (NFR-2 vs FR-7):** Status: Partial — as built, writing is deterministic pretty-printed output, not byte-for-byte preservation of untouched regions. Comments and elements outside the six modeled types are not preserved (see README Known limitations). Determinism holds for the written output; atomic-write guards leave the original intact on failure.
- **Full-file export default:** Status: Planned — not yet implemented. As built, the command line prints human text or a single JSON envelope to stdout (`--json` before the subcommand); there is no `<name>.parsex.json` sidecar, `output.savedPath` / `output.summary`, or `--force` flag.
- **Core implementation language:** Status: Implemented — C++20 (`libparsex` + two surfaces). The earlier Rust recommendation is superseded.
- `PROJECT_BRIEF.md`: confirmed intentionally removed, not needed for this project.

## Scope — v1

### Feature: Parsing & multi-file resolution
Status: Implemented (single-file; multi-file project resolution: Planned).

Description: parse ARXML files (single or multi-file projects) covering the CAN slice of the Classic Platform meta-model, within the supported AUTOSAR release range.

**Acceptance criteria:**
- Given a valid ARXML file in the 4.2.2–R21-11 range, when it's parsed, then it produces a structured model with the AUTOSAR schema release auto-detected (FR-5, FR-9).
- Given a multi-file ARXML project, when parsed, then short-name references across files resolve correctly (FR-8).
- Given a file outside the supported release range, when parsed, then ParseX returns a clear "unsupported AUTOSAR release" error rather than attempting a lossy best-effort parse.
 - Given `--strict` is passed to `parsex validate` (as built; the draft's `parsex parse --strict` was not implemented), when the file fails schema validation, then the report marks failure and no model is produced for downstream steps.

**Edge cases considered:**
- File claims a release inside the supported range but is otherwise malformed → handled under Validation (NFR-3), not silently accepted.

### Feature: Round-trip fidelity & safe write-back
Status: Partial — deterministic rewrite with documented non-passthrough limitation (see above); atomic-write failure safety: Implemented.

Description: writing a parsed (and optionally edited) model back to ARXML must not lose or corrupt anything ParseX doesn't explicitly model, and must never leave a corrupted file behind on failure.

**Acceptance criteria:**
- Given a file is parsed and written back with no changes, when compared to the original, then no content ParseX doesn't explicitly model is lost (FR-7, NFR-1).
- Given a targeted edit (e.g. changing one signal's `FACTOR` from `0.25` to `0.5`), when written back, then only that element's bytes change — all untouched regions (indentation, comments, element order, unrelated elements) remain byte-identical to the original. Example:
  - Original: `<FACTOR>0.25</FACTOR>` inside a file with a tool-generated `<!-- exported by ... -->` comment and 2-space indentation.
  - After edit: only `<FACTOR>0.5</FACTOR>` changes; the comment, indentation, and everything else stay exactly as they were.
  - This is deliberately chosen over full-file re-normalization on every write (which would give a "purer" determinism story but silently drop untracked content like comments and produce noisy diffs) — see spec discussion for the full comparison.
- Given the same input file and the same edit is applied, when written back, then the output bytes are identical every time (determinism holds for the written regions) (NFR-2).
- Given a write is interrupted or fails, when the operation ends, then the original file is left intact — no partial or corrupted output in its place (NFR-4).

**Edge cases considered:**
- Edit touches an element ParseX doesn't fully model (e.g. an unrecognized child element under a signal) → the write must fail closed (reject the edit) rather than silently dropping the unrecognized content.

### Feature: Validation
Status: Implemented — schema + semantic checks with strict/lenient modes and safe hostile-XML handling.

Description: schema and semantic validation of ARXML files, with actionable error reporting.

**Acceptance criteria:**
- Given a file, when validated, then XSD schema violations are reported with file and element path context (FR-10).
- Given a file, when semantic validation runs, then dangling references and duplicate short names are reported as part of a configurable rule set, not an all-or-nothing check (FR-11).
- Given any input, including malformed or deliberately hostile XML, when parsed, then the parser rejects or safely ignores XXE and other known XML-parsing attack patterns rather than executing them (NFR-3).

### Feature: Semantic diff
Status: Implemented — model-level added/removed/moved/modified report with deterministic text and JSON rendering.

Description: compare two revisions of an ARXML file and describe meaningful model-level changes.

**Acceptance criteria:**
- Given two revisions of an ARXML file, when diffed, then the result describes meaningful model changes (e.g. "signal EngineSpeed's factor changed from 0.25 to 0.5"), not a raw text/line diff (FR-12).
- Given a diff request, when run via CLI, then output is available in both human-readable and JSON form (FR-13).

### Feature: CLI
Status: Implemented as `parse`, `validate`, `diff`, `write` subcommands with `--json` (before the subcommand), `--strict` on validate, dry-run-by-default write, sysexits exit codes, `--config` / `--schema-cache-dir` precedence, and `--trace` timing. Planned (not implemented): file-export sidecar, `--force`, `parse --strict`.

Description: `parsex` command-line interface exposing parse, validate, diff, and write operations with structured output.

**Acceptance criteria:**
- Given `parsex parse|validate|diff`, when run, then structured JSON output is available alongside human-readable output (FR-1, FR-2).
 - Given a full-file parse request, when run with `--json` before the subcommand, then the result is a single JSON envelope on stdout (not a sidecar file), and human-readable output otherwise. Planned (not implemented): `<name>.parsex.json` sidecar with `output.savedPath` / `output.summary` and `--force` overwrite guard.
 - Given `parsex validate --strict` (as built), when run, then schema conformance is checked as part of validation — findings live in the report body, not the exit code.

### Feature: MCP server — read tools
Status: Implemented as `parse_arxml` / `validate_arxml` / `diff_arxml` over stdio JSON-RPC (protocol `2026-07-28`, stateless `_meta` version per request). The draft's per-object list/get tool shape was not implemented; each tool returns the same report envelope the command line emits.

Description: assistant tools exposing parse, validate, and diff, read-only by default.

**Acceptance criteria:**
- Given the MCP server starts with no flag, when a client connects, then only read tools are available — list/get for Cluster, EcuInstance, Frame, Pdu, Signal, SignalGroup (FR-3, FR-14, FR-15).
 - Given any read tool response, when returned, then it follows the shared envelope (`$schema`, `contractVersion`, `kind`, `payload`, `toolVersion`; see `schemas/`). The draft's `JSON_OUTPUT_SHAPE.md` (`version`, `objectType`, `source`, `data`, `warnings`) is superseded.

### Feature: MCP server — write tools
Status: Implemented as `write_arxml` with dry-run default and explicit apply. The draft's server-startup flag and separate confirmation-call shape were not implemented.

Description: explicitly-confirmed tool for making validated edits.

**Acceptance criteria:**
 - Given `write_arxml` is called without applying (as built), when it runs, then it returns a dry-run preview and writes nothing; a separate explicit apply request persists the change (as built).
 - Given a write is applied, when it runs, then the result is validated before persisting; a validation failure blocks the write and leaves the file untouched (FR-16, NFR-4).

### Feature: JSON output contract
Status: Implemented — single envelope (`$schema`, `contractVersion` `1.0.0`, `kind`, `payload`, `toolVersion`) with kinds `parseReport`, `validationResult`, `diffReport`, `writeResult`, `telemetryReport`; optional fields use omit (see `schemas/`). The draft's `JSON_OUTPUT_SHAPE.md` field names are superseded.

Description: the common response envelope used by both command line and assistant server.

**Acceptance criteria:**
 - All command-line / assistant responses use the envelope in `schemas/` (`contractVersion` tracked separately from the file's `autosarRelease`; `toolVersion` from the single version source), with exactly the payload for the report `kind`, plus `warnings` where applicable.
 - Protocol-specific fields live under a nested `protocolSpecific` object so common fields stay stable if/when non-CAN protocols are added later — the envelope is protocol-agnostic even though v1 only implements CAN objects.

## Cross-cutting non-functional requirements

- Runs identically (no OS-specific parsing/validation differences) on Linux, macOS, and Windows (NFR-6).
- Error and validation messages are understandable without deep AUTOSAR meta-model expertise (NFR-7) — checked during self-review (Phase 7), not just automated tests.
- A regression corpus of real or realistically representative ARXML files backs round-trip testing (NFR-8).

## Out of scope (for now)

- AUTOSAR Adaptive Platform — different meta-model entirely, no v1 need.
- Non-CAN protocols (LIN, FlexRay, Ethernet/SOME-IP) — envelope designed to extend to them, but no implementation in v1.
- Dedicated CI/CD product surface (CON-1) — validation/diff stay generic enough for third parties to wire in themselves.
- ISO 26262 tool-qualification claims (CON-2) — not relevant until/unless the project is used in safety-relevant pipelines.
- Formal governance/contribution process — solo-maintained for now; revisit if outside contributors show up.
- MCP client-specific certification — generic/spec-compliant testing only for v1.
- Formal error-response JSON shape, pagination for large list responses, closed enum value sets for fields like `category`/`byteOrder` — real gaps, deferred to Phase 3 task breakdown rather than blocking this spec.

## Open questions

- ~~Error-response JSON shape (distinct from the success/warnings shape here) — not yet defined.~~ Resolved in Phase 3 (LLD): see `docs/lld.md`, Component: JSON Output Contract — `ErrorEnvelope` shape, with `error.code` values mapping to the error types defined across Parser/Validator/Write Engine's LLD error handling tables.
- ~~Omit-vs-null convention for optional JSON fields — JSON_OUTPUT_SHAPE.md recommends omit; not yet locked as final policy.~~ Resolved in Phase 3 (LLD): see `docs/lld.md`, Component: JSON Output Contract — **omit** is the locked policy, applied uniformly across all optional fields.
- Governance/contribution model — deferred until relevant.
- Core implementation language — deferred to Phase 2 (architecture); Rust is the standing recommendation (CON-7).
