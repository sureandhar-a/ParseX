# ParseX
ParseX — a protocol-agnostic ARXML (AUTOSAR XML) parsing, validation, diffing, and safe editing toolkit, covering the full CAN communication stack on AUTOSAR Classic Platform (releases 4.2.2 through R21-11).

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
