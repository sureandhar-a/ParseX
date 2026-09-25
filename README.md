# ParseX

[![CI](https://github.com/sureandhar-a/ParseX/actions/workflows/ci.yml/badge.svg)](https://github.com/sureandhar-a/ParseX/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

ParseX is an ARXML parsing, validation, diffing, and safe editing toolkit built specifically for AUTOSAR XML semantics. Unlike a generic XML tool, it understands AUTOSAR schema versions through a shared schema registry and exposes the same core engine through both a scriptable command line and an assistant-facing server.

ParseX is for engineers working with AUTOSAR Classic Platform files who need trustworthy automation: parsing files into a typed domain model, validating them against official schemas and CAN rules, comparing two files structurally, and writing changes back safely with a dry-run by default.

## Contents

- [Quickstart](#prerequisites)
- [Building](#build)
- [Documentation](#documentation)
- [Contributing](CONTRIBUTING.md)
- [License](#license)

## Prerequisites

- A recent C++20 compiler (Clang or GCC — continuous integration covers Ubuntu, macOS, and Windows).
- CMake 3.21 or newer.
- Ninja build tool (the configured presets use the Ninja generator).
- vcpkg is vendored as a submodule — clone with `--recurse-submodules` so dependency resolution works for the core XML, JSON, and command-line libraries.

On macOS, install the host build tool `pkg-config` before running CMake/vcpkg:

```bash
brew install pkg-config
```

This is required because vcpkg uses `pkg-config` during dependency configuration for packages such as `cli11` and `libxml2`.

## Schemas (user-supplied)

ParseX validates against the official AUTOSAR XSD schemas, which are
copyrighted and therefore downloaded onto your machine, never shipped with
the repo. Fetch them with the setup script (stdlib-only, hash-verified):

```bash
python3 scripts/download_schemas.py
```

See [`resources/schemas/README.md`](resources/schemas/README.md) for details
and the manual fallback.

## Build

```bash
git clone --recurse-submodules https://github.com/sureandhar-a/ParseX.git
cd ParseX
cmake --preset default
cmake --build --preset default
```

This produces two working binaries:

- `build/debug/parsex-cli/parsex-cli` — the scriptable command line
- `build/debug/parsex-mcp/parsex-mcp` — the assistant-facing server

Run tests with the preset:

```bash
ctest --preset default
```

Full testing strategy (unit, sanitizers, fuzz, stability, shared output,
slow, coverage): [docs/testing.md](docs/testing.md).

Optional build flags (see the testing guide for when to use each):

- `PARSEX_ENABLE_SANITIZERS` — address and undefined-behavior checks
- `PARSEX_ENABLE_FUZZING` — libFuzzer harnesses (needs Clang with the runtime)
- `PARSEX_ENABLE_COVERAGE` — coverage instrumentation for the floor-guarded report

You can also build the release preset:

```bash
cmake --preset release
cmake --build --preset release
```

## First run

Parse the bundled sample and validate it. All commands below were run against the current build and show real output:

```bash
build/debug/parsex-cli/parsex-cli parse --input examples/sample.arxml
# Parsed examples/sample.arxml
#   release: 4.2.2
#   clusters: 0
#   ecuInstances: 0
#   frames: 0
#   pdus: 0
#   signals: 0
#   signalGroups: 0

build/debug/parsex-cli/parsex-cli validate --input examples/sample.arxml --strict
# Validation passed (0 issues)
```

See what a failure looks like with the deliberately broken companion file:

```bash
build/debug/parsex-cli/parsex-cli validate --input examples/sample_invalid.arxml
# Validation failed (1 issues)
#   [error] schema.invalid: Element '{http://autosar.org/schema/r4.0}AR-PACKAGE': Missing child element(s). Expected is ( ... SHORT-NAME ).
```

For machine-readable output, put `--json` before the subcommand (see [command-line reference](docs/cli.md) for the full shape — the schema itself lives with the output contract and is not repeated here):

```bash
build/debug/parsex-cli/parsex-cli --json parse --input examples/sample.arxml
```

Note: richer study-vocabulary fixtures such as `tests/fixtures/parsefile_complete.arxml` parse with real domain counts but intentionally fail official-schema validation. That gap is documented under Known limitations below.

## Documentation

- [Requirements and scope](docs/spec.md) — what the toolkit covers and why.
- [High-level design](docs/hld.md) — components and data flow.
- [Low-level design](docs/lld.md) — per-component interfaces.
- [Command-line reference](docs/cli.md) — every flag, subcommand, and exit code.
- [Assistant server reference](docs/mcp.md) — tools, protocol version, and launch config.
- [Testing guide](docs/testing.md) — how to run unit, sanitizer, fuzz, stability, and coverage checks.
- [Shared types module reference](docs/shared-types.md) — the domain types, containers, and protocol-extension variant: which file to open for what.
- [Schema Registry module reference](docs/schema-registry.md) — schema sources, disk cache, shared-handle contract, and the redistribution caveat.
- [Output contract versioning](schemas/VERSIONING.md) — how report shapes evolve.
- [Design decisions](docs/decisions/) — composition over inheritance, vendored schemas, byte-offset tracking.

## Repository layout

- `libparsex/` — core C++ library (parsing, validation, comparison, writing, shared output, timing).
- `parsex-cli/` — scriptable command-line surface around the core library.
- `parsex-mcp/` — assistant-facing server around the same core library.
- `tests/` — unit and hardening checks with shared fixtures.
- `fuzz/` — libFuzzer harnesses and seed corpus.
- `docs/` — user and design documentation.
- `examples/` — small runnable samples for the first-run walkthrough.
- `schemas/` — versioned report shapes and output contract notes.
- `scripts/` — setup, coverage, and maintenance helpers.
- `resources/` — user-supplied schema layout and notes.

## Known limitations (Write Engine)

The Write Engine faithfully round-trips everything the domain model
represents, but it is schema-typed, not a general passthrough: XML comments
and elements/attributes outside the six modeled types (Cluster, EcuInstance,
Frame, Pdu, Signal, SignalGroup) are not preserved on a load-then-write
round trip. Study-vocabulary REF details with no single-node schema-valid
home (ECU channel refs, frame transmitters, signal receivers/value tables)
are likewise dropped. This mirrors the `autosar-data` crate's own
non-passthrough design and is covered by explicit tests, not discovered by
accident.

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.
