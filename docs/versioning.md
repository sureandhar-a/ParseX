# Versioning policy

ParseX follows [Semantic Versioning 2.0.0](https://semver.org/) with the
version expressed as `major.minor.patch`. The first public release is
`0.1.0`. The contract shape version (`contractVersion`, see
`schemas/VERSIONING.md`) moves with the project version for the v1 milestone;
its own breaking-change table still applies when judging whether a change is
breaking.

## What requires which bump

Answer "does change X require a major, minor, or patch bump" per surface.
When a change touches more than one surface, take the highest bump any
touched surface requires.

### Command line

Breaking (major after `1.0.0`; minor while pre-`1.0.0`, see below):

- Removing or renaming a subcommand (`parse`, `validate`, `diff`, `write`).
- Removing or renaming a flag (`--json`, `--strict`, `--apply`, `--output`,
  `--schema-cache-dir`, `--config`, `--trace`, `--no-color`, `--version`).
- Changing what an exit code means (exit-code meanings are set by the
  classifier behind the single top-level error boundary).
- Making a previously accepted invocation fail, or changing human-readable
  output in a way scripts reasonably parse (the JSON envelope is the stable
  machine surface; see next section).

Minor (backward-compatible addition):

- A new subcommand or a new optional flag that changes nothing when absent.
- A new optional config key with a safe default.

Patch:

- Help-text wording, error-message wording with the same meaning and exit
  code, documentation-only changes.

### Assistant tools

Breaking:

- Removing or renaming a tool (`parse_arxml`, `validate_arxml`,
  `diff_arxml`, `write_arxml`).
- Removing, renaming, or narrowing a tool input/output schema field, or
  changing an annotation clients rely on.
- Changing the two-tier error split (transport-level JSON-RPC `error` vs.
  tool-result `isError:true`) so callers must reclassify failures.

Minor:

- A new optional tool, a new optional input field, or a new optional output
  field that old clients safely ignore.

Patch:

- Description or annotation wording with no schema effect.

### Shared report envelope

Breaking:

- Removing or renaming an envelope field (`$schema`, `contractVersion`,
  `kind`, `payload`, `toolVersion`) or a report payload field consumers read.
- Removing a report `kind` or changing what an existing `kind` means.
- Dropping support for an AUTOSAR schema release in the supported range
  (a schema-support regression breaks previously passing inputs).

Minor:

- A new optional envelope or payload field, a new report `kind`, or a new
  enum value that old readers safely ignore.

Patch:

- Typo fixes in descriptions or examples with no validator-visible change
  (mirrors `schemas/VERSIONING.md`).

## Pre-1.0 carve-out

Per SemVer 2.0.0 §4, before `1.0.0` minor bumps may include breaking changes
and patch bumps are for compatible fixes only. In practice until `1.0.0`:

- `0.x.y` → `0.(x+1).0` for anything that would be major after `1.0.0`,
  plus for ordinary backward-compatible additions.
- `0.x.y` → `0.x.(y+1)` for compatible fixes only.

## Graduating to 1.0.0

`1.0.0` is reserved for the point where all six functional areas (parsing,
validation, comparison, writing, command line, assistant server) plus quality
hardening and this documentation and release work are complete and stable.
After `1.0.0`, the standard SemVer rules apply with no carve-out: breaking
changes require a major bump.

## Bumping the version (maintainer process)

Follow these steps in order ahead of any release. Each step was dry-run
against the current tree so nothing here is assumed.

1. Decide the new version using the bump table above.
2. Edit the single version source: the `VERSION` in the top-level
   `CMakeLists.txt` `project(...)` declaration. Reconfigure so the
   generated `parsex/version_config.hpp` picks it up, then confirm the
   command line `--version`, a report envelope's `toolVersion` /
   `contractVersion`, and the packaging manifest all report the new value.
3. Move `CHANGELOG.md`'s `## [Unreleased]` content into a new dated
   `## [x.y.z] - YYYY-MM-DD` section, leaving an empty `## [Unreleased]`
   behind for the next cycle.
4. Update the `"version"` field in the packaging manifests (`vcpkg.json`,
   and the overlay port manifest once it exists) to the same value.
5. Commit the version source, changelog, and manifest changes together as
   one commit (`chore: bump version to x.y.z`).
6. Tag the bump commit as `vX.Y.Z` (annotated tag). The tag format and the
   notes template are defined in the release process; this process and that
   one agree on `vX.Y.Z` so the port file fetch reference and the release
   automation trigger on the same string.

Do not tag or publish from this process alone — tagging belongs to the
pre-release checklist, which re-verifies the gates below before anything is
cut.

## Release gates

A version bump is only half the release. Before cutting any release, the
[testing guide](testing.md) suite must pass and the coverage floor enforced
by `scripts/check_coverage_floor.py` (currently 35%) must hold. The full gate
list lives in the pre-release checklist.
