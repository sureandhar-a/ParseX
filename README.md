# ParseX
ParseX — a protocol-agnostic ARXML (AUTOSAR XML) parsing, validation, diffing, and safe editing toolkit, covering the full CAN communication stack on AUTOSAR Classic Platform (releases 4.2.2 through R21-11).

## Prerequisites

On macOS, install the host build tool `pkg-config` before running CMake/vcpkg:

```bash
brew install pkg-config
```

This is required because vcpkg uses `pkg-config` during dependency configuration for packages such as `cli11` and `libxml2`.

## Build

```bash
cmake -B build -S .
cmake --build build
```
