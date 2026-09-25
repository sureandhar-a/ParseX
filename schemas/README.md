# ParseX JSON Schemas

Single source of truth for ParseX's JSON Output Contract (PAR-160). An external
tool consuming ParseX's JSON output should start here — no application source
reading required.

## Files

* [`envelope.schema.json`](envelope.schema.json) — top-level envelope
  (`$schema`, `contractVersion`, `toolVersion`, `kind`, `payload`). Draft 2020-12.
  Closed top-level set (`additionalProperties: false`); per-kind `payload`
  shape selected by the `kind` discriminator via `allOf`/`if`/`then` `$ref`s.
* [`validation_result.schema.json`](validation_result.schema.json) — `kind:
  "validationResult"` payload: `passed` flag plus the `errors` array
  (`severity`, `code`, `message`, optional `location`/`path`/`expectedType`/
  `actualType`).
* [`diff_report.schema.json`](diff_report.schema.json) — `kind: "diffReport"`
  payload: `entries` (`kind`, `elementType`, optional `oldPath`/`newPath`,
  `fieldDiffs`) plus `diagnostics`.
* [`telemetry_report.schema.json`](telemetry_report.schema.json) — `kind:
  "telemetryReport"` payload: flat `spans` array (`spanId`, optional
  `parentSpanId`, `name`, `startNanos`/`endNanos`/`durationNanos`, `status`,
  `attributes`). Local-only instrumentation — see
  [`TELEMETRY_SCOPE.md`](TELEMETRY_SCOPE.md) for the deliberate no-phone-home
  scoping decision.
* [`CONVENTIONS.md`](CONVENTIONS.md) — field-naming (camelCase), null-handling
  (omit, don't null), and forward-compatibility (consumers ignore unknown
  fields) rules every payload schema is reviewed against.
* [`VERSIONING.md`](VERSIONING.md) — `contractVersion` semver scheme
  (major = breaking, minor = additive, patch = docs-only) and the
  breaking-vs-non-breaking classification table.

## Design note: `additionalProperties: false` at the envelope level (PAR-166)

The envelope schema is **closed**: top-level envelope keys are a fixed set — ParseX as a
producer will not emit stray top-level keys outside `$schema`, `contractVersion`,
`toolVersion`, `kind`, `payload`.

This is a deliberately different stance from the consumer-side tolerance principle in
[`CONVENTIONS.md`](CONVENTIONS.md): consumers reading ParseX's output (including ParseX's own
future code reading its own JSON back in) **must ignore unknown fields** so a future
minor version adding a field does not break them.

In other words: the producer schema is closed today and will be relaxed (`additionalProperties`
revisited) in a later minor version per [`VERSIONING.md`](VERSIONING.md) rules; consumers must
already be lenient. This asymmetry is intentional and documented here so the two docs are not read
as contradictory.

## Quick example

A validation result envelope (`kind: "validationResult"`):

```json
{
    "$schema": "https://parsex.dev/schemas/v1/envelope.json",
    "contractVersion": "0.1.0",
    "toolVersion": "0.1.0",
    "kind": "validationResult",
    "payload": {
        "passed": false,
        "errors": [
            {
                "severity": "error",
                "code": "can.dlc_mismatch",
                "message": "DLC 8 != 64",
                "path": "/Cluster/CAN/Frame"
            }
        ]
    }
}
```

A diff report envelope (`kind: "diffReport"`):

```json
{
    "$schema": "https://parsex.dev/schemas/v1/envelope.json",
    "contractVersion": "0.1.0",
    "toolVersion": "0.1.0",
    "kind": "diffReport",
    "payload": {
        "entries": [
            {
                "kind": "modified",
                "elementType": "Frame",
                "newPath": "/F/Old",
                "oldPath": "/F/Old",
                "fieldDiffs": [
                    {"field": "length", "oldValue": "8", "newValue": "64"}
                ]
            }
        ],
        "diagnostics": []
    }
}
```

In C++, build either envelope with the shared helper instead of hand-rolling it:

```cpp
#include <parsex/json_contract/envelope.hpp>

nlohmann::json out =
    parsex::json_contract::wrapEnvelope("validationResult", std::move(payload));
```
