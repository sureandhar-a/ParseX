# Changelog

All notable user-facing changes to this project are documented in this file.

The format follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/):
an `## [Unreleased]` section on top for changes since the last release, then
one `## [x.y.z] - YYYY-MM-DD` section per release with `Added`, `Changed`,
`Fixed`, and `Removed` subsections. See `docs/versioning.md` for what each
bump means.

## [Unreleased]

## [0.1.0] - 2026-09-25

### Added

- Shared schema registry resolving the AUTOSAR release per file to a local
  user-supplied schema with on-disk caching and a shared-handle contract.
- Typed domain model for the CAN stack (cluster, ECU instance, frame, PDU,
  signal, signal group) with shared containers and protocol-extension slot.
- File parsing into the typed model with release auto-detection, secure
  reading, and hostile-input rejection.
- Schema and semantic validation with file and element path context and
  strict/lenient modes.
- Structural comparison reporting added, removed, moved, and modified
  entries with deterministic text and JSON rendering.
- Safe write-back producing deterministic schema-valid output with
  validation before persisting and atomic replacement on failure
  (dry-run by default).
- Versioned shared report envelope for every machine-readable response
  with naming, null-handling, and compatibility conventions.
- Local-only timing instrumentation with auto-nesting spans and
  zero overhead when disabled, surfaced as a timing report on request.
- Scriptable command line with parse, validate, diff, and write
  subcommands, human-readable and machine-readable output, stable exit
  codes, flag/env/config/default precedence, and per-run timing.
- Assistant-facing stdio server exposing parsing, validation, comparison,
  and safe writing as callable tools with registry, schemas, and
  dry-run-by-default writes.
- Quality hardening: curated edge-case suites, time-bounded fuzz smoke
  checks, parse-write-reparse stability and comparison convergence,
  version-boundary and setup-failure checks, shared-type guarantees,
  consistent report and timing checks, enforced coverage floor, and one
  ordered pipeline with a testing guide.
- Release-readiness docs: quickstart readme with verified samples,
  reconciled requirements and design docs, versioning policy, contribution
  and setup guides, packaging port, and repeatable release process.

## Contributing to this file

Every user-facing pull request adds an entry under `## [Unreleased]` above
(new section if the change type has none yet). The review template checks
for it, so missing entries fail review rather than slipping through.
