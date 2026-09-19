# AUTOSAR XSD schemas (user-supplied, never committed)

ParseX validates against the official AUTOSAR Classic Platform XML Schemas,
but **the `.xsd` files themselves are not part of this repository**. They are
AUTOSAR-copyrighted material whose terms permit personal informational use
only:

> "For any other purpose [besides personal informational use], no part of the
> work may be utilized or reproduced, in any form or by any means, without
> permission in writing from the publisher."

Redistributing the raw XSDs in a (public) repo counts as unauthorized
reproduction — so each user downloads them from AUTOSAR onto their own
machine (Strategy 1: user-supplied schema model). The repo stays 100% legal;
the responsibility to obtain the schemas falls on the user. Background and
the rejected alternative (vendoring) are recorded in
`docs/decisions/0002-vendored-schemas-no-runtime-fetch.md`.

## Lookup contract (what the code expects)

`SchemaRegistry::resolveSchema(release)` looks for **one file per release,
named exactly by the release string**:

| Lookup key (filename) | AUTOSAR release | File to place here (extracted from AUTOSAR's archive) |
| --- | --- | --- |
| `4.2.2.xsd` | Classic 4.2.2 | `AUTOSAR_4-2-2.xsd` |
| `4.3.0.xsd` | Classic 4.3.0 | `AUTOSAR_4-3-0.xsd` |
| `4.3.1.xsd` | Classic 4.3.1 | `AUTOSAR_00044.xsd` |
| `4.4.0.xsd` | Classic 4.4.0 | `AUTOSAR_00046.xsd` |
| `R19-11.xsd` | R19-11 (internal 4.5.0) | `AUTOSAR_00048.xsd` |
| `R20-11.xsd` | R20-11 (internal 4.6.0) | `AUTOSAR_00049.xsd` |
| `R21-11.xsd` | Classic R21-11 (internal 4.7.0) | `AUTOSAR_00050.xsd` |

ParseX covers Classic Platform **4.2.2 – R21-11** (see root `README.md`).
Resolution order the Schema Registry will implement:

1. `--xsd-path <dir>` (CLI flag, highest precedence),
2. `$PARSEX_SCHEMA_DIR` (environment),
3. the built-in default: `<install-prefix>/share/parsex/schemas` (populated
   by `cmake --install` from this directory), falling back to the build tree
   `build/<cfg>/resources/schemas` for local runs and tests.

Do not reformat or subset the files: byte-faithful copies keep hashes
meaningful. All seven are well-formed XML with root `xs:schema` and
`targetNamespace="http://autosar.org/schema/r4.0"`.

## How to obtain the schemas (each user, on their machine)

The standalone `AUTOSAR_MMOD_XMLSchema.zip` archives are publicly reachable
on autosar.org — no account needed for these (the login wall guards the full
release *packages*, not the schema archives):

| Release | Archive URL |
| --- | --- |
| 4.2.2 | `https://www.autosar.org/fileadmin/standards/R4.2.2/CP/AUTOSAR_MMOD_XMLSchema.zip` |
| 4.3.0 | `https://www.autosar.org/fileadmin/standards/R4.3.0/CP/AUTOSAR_MMOD_XMLSchema.zip` |
| 4.3.1 | `https://www.autosar.org/fileadmin/standards/R4.3.1/CP/AUTOSAR_MMOD_XMLSchema.zip` |
| 4.4.0 | `https://www.autosar.org/fileadmin/standards/R18-10_R4.4.0_R1.5.0/CP/AUTOSAR_MMOD_XMLSchema.zip` (combined Adaptive/Classic/Foundation release path) |
| R19-11 | `https://www.autosar.org/fileadmin/standards/R19-11/CP/AUTOSAR_MMOD_XMLSchema.zip` |
| R20-11 | `https://www.autosar.org/fileadmin/standards/R20-11/FO/AUTOSAR_MMOD_XMLSchema.zip` (unified post-R19-11 schema, Foundation-hosted) |
| R21-11 | `https://www.autosar.org/fileadmin/standards/R21-11/FO/AUTOSAR_MMOD_XMLSchema.zip` |

Steps: download the archive(s) for the release(s) you need, extract the
`.xsd`, rename it to the lookup key from the table above, and drop it in this
directory (or any directory you then pass via `--xsd-path` /
`$PARSEX_SCHEMA_DIR`). Then re-run configure — CMake copies `*.xsd` into the
build tree and the install output (see root `CMakeLists.txt`).

`xml.xsd` (committed, W3C-licensed — not AUTOSAR-copyrighted, per the R21-11
release overview §1.2.2) must sit alongside the schemas: every AUTOSAR XSD
imports it via relative `schemaLocation="xml.xsd"`, and parsing fails without
it. The copy here is byte-identical across all six archives that ship it
(R19-11's archive omits it; the same file applies — the xml namespace is
version-independent).

Reference (no login needed): the R21-11 release overview PDF lists exact
schema/version numbers:
https://www.autosar.org/fileadmin/standards/R21-11/CP/AUTOSAR_TR_ClassicPlatformReleaseOverview.pdf
Release index: https://www.autosar.org/standards/classic-platform

## Local provenance (developer's machine, 2026-09-19 — not committed)

The seven archives above were downloaded from autosar.org (Playwright-driven
navigation + curl, no login) and extracted byte-faithfully (extracted file
SHA256 == archive content SHA256 for all seven). These local copies stay
outside version control per `.gitignore`.

## Licensing (decision, not a TODO)

Vendoring the raw XSDs was evaluated and **rejected**: AUTOSAR's terms
quoted above prohibit redistribution without written permission, a bar this
project cannot clear on its own. This directory therefore tracks only the
layout, this README, and the build wiring — never `*.xsd` (gitignored).
Pre-compiling/abstracting schema data into committed generated code
(Strategy 2, cf. `autosar-data-specification`) remains a possible future
step; it changes what is derived, not what is downloaded, and needs its own
legal review before adoption.
