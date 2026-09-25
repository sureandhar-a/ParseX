# ParseX

[![CI](https://github.com/sureandhar-a/ParseX/actions/workflows/ci.yml/badge.svg)](https://github.com/sureandhar-a/ParseX/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](CHANGELOG.md)
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

Using the CMake preset:

```bash
cmake --preset default
cmake --build --preset default
```

Run tests with the preset:

```bash
ctest --preset default
```

Full testing strategy (unit, sanitizers, fuzz, stability, shared output,
slow, coverage): [docs/testing.md](docs/testing.md).

You can also build the release preset:

```bash
cmake --preset release
cmake --build --preset release
```

## Documentation

- [Shared types module reference](docs/shared-types.md) — the domain types, containers, and protocol-extension variant: which file to open for what.
- [Schema Registry module reference](docs/schema-registry.md) — schema sources, disk cache, shared-handle contract, and the redistribution caveat.

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
