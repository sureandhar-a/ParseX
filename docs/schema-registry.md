# Schema Registry

The Schema Registry (`libparsex/include/parsex/schema/`) resolves the
correct AUTOSAR XSD schema for a release string (e.g. `"4.2.2"`,
`"R21-11"`), caches it on disk, and hands back one shared schema handle
(`CachedSchema`) that both the Parser and the Validator use — so each XSD
is loaded and parsed once per release, not once per component.

Rule of thumb: resolving a release → `schema_registry.hpp`; the handle's
lifetime and sharing rules → `cached_schema.hpp`; failures →
`schema_resolution_error.hpp`; where files live on disk → below.

## Schema sources: user-supplied, never fetched, never committed

Schemas live under `resources/schemas/<release>.xsd`, one file per release
named exactly by the lookup key (`4.2.2.xsd` … `R21-11.xsd`), plus the
W3C-licensed `xml.xsd` every schema imports. They are **user-supplied**:
run `python3 scripts/download_schemas.py` and each file is fetched from
autosar.org, hash-verified, and installed (see `resources/schemas/README.md`
for the URL table, per-release path quirks, and the manual fallback). Nothing is fetched at runtime: resolution order is `$PARSEX_SCHEMA_DIR`
(or a future `--xsd-path`), then the install default, then the source tree
(see `docs/decisions/0002-vendored-schemas-no-runtime-fetch.md` for why).

## Redistribution caveat (read before any public release)

The raw XSDs are AUTOSAR-copyrighted: beyond personal informational use, "no
part of the work may be utilized or reproduced, in any form or by any means,
without permission in writing from the publisher." That is why `*.xsd` is
gitignored and the repo stays 100% legal — and why this must be re-checked
deliberately (not assumed away) before open-sourcing. This paragraph is
repeated here rather than only in `resources/schemas/README.md` because this
page is where future-you will look first.

## Disk cache

Resolved schemas are cached at `$XDG_CACHE_HOME/parsex/schemas/` (or
`~/.cache/parsex/schemas/`), one `<release>.xsd.cache` per release plus a
staged `xml.xsd` sidecar, written atomically (temp file + fsync + rename —
a kill mid-write can never leave a half-written entry). The cache is
**safe to delete entirely at any time**: the next use just re-copies from
the source file. Deleting it can never cause data loss or corruption.

## Sharing and thread safety

`resolveSchema()` returns one shared handle per release (see
`CachedSchema`), held by `shared_ptr` so neither Parser nor Validator
worries about who frees it last. The thread-safety contract — schema
shareable, validation contexts strictly per-caller — lives as a doc comment
on `CachedSchema` in `cached_schema.hpp`; read that, not a retelling here.
Today's single-threaded CLI satisfies it trivially; future parallel workers
must mint their own contexts from the shared schema.
