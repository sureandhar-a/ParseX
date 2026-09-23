# ParseX JSON Schemas

Single source of truth for ParseX's JSON Output Contract (PAR-160).

## Files

* `envelope.schema.json` — top-level envelope (`$schema`, `contractVersion`, `toolVersion`, `kind`, `payload`). Draft 2020-12.
* `validation_result.schema.json` — `validationResult` payload (PAR-173).
* `diff_report.schema.json` — `diffReport` payload (PAR-173).
* `CONVENTIONS.md` — naming / null-handling / compatibility rules (PAR-162).
* `VERSIONING.md` — `contractVersion` semver + breaking-change table (PAR-163).

## Design note: `additionalProperties: false` at the envelope level (PAR-166)

The envelope schema is **closed**: top-level envelope keys are a fixed set — ParseX as a
producer will not emit stray top-level keys outside `$schema`, `contractVersion`,
`toolVersion`, `kind`, `payload`.

This is a deliberately different stance from the consumer-side tolerance principle in
`CONVENTIONS.md` (PAR-162): consumers reading ParseX's output (including ParseX's own
future code reading its own JSON back in) **must ignore unknown fields** so a future
minor version adding a field does not break them.

In other words: the producer schema is closed today and will be relaxed (`additionalProperties`
revisited) in a later minor version per `VERSIONING.md` rules; consumers must already be
lenient. This asymmetry is intentional and documented here so the two docs are not read
as contradictory.
