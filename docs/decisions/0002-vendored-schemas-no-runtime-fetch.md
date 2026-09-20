# 0002 — Vendored schemas, no runtime fetching

> SUPERSEDED 2026-09-19 by the amendment at the bottom: raw XSDs are
> user-supplied and never committed (Strategy 1). The no-runtime-fetching
> half of this decision stands unchanged.

- Context: the Schema Registry feature text justifies vendoring with three
  ecosystem precedents: Eclipse Artop ships release-specific schema artifacts
  in its distribution; the Rust autosar-data project "checks in known-release
  schemas as a dedicated crate"; the Python cogu/autosar project "resolves
  against a fixed set of internally known versions".

- Correction recorded during vendoring (2026-09-19): the autosar-data citation
  as written is inaccurate. That project explicitly does *not* redistribute
  the XSDs — its `autosar-xsd-mangler` README states "The required xsd files
  are not provided here, since their copyright does not allow for
  redistribution", and the `autosar-data-specification` crate contains only
  *generated Rust tables derived from* the XSDs. Likewise, cogu/autosar
  hardcodes version knowledge (e.g. `50 = R21-11/Classic 4.7`) rather than
  shipping schema files. Only the Artop precedent is a true same-solution
  precedent. The conclusion below stands, but on corrected grounds — and the
  correction strengthens the licensing TODO rather than weakening the
  decision.

- Decision: ParseX checks a fixed set of official `.xsd` files into
  `resources/schemas/` (one per release, named by lookup key) and never
  fetches schemas at runtime. Verified 2026-09-19: no network/fetch code
  exists anywhere in first-party sources (`libparsex/`, `parsex-cli/`,
  `tests/` — grep for curl/wget/http/download/fetch returns nothing outside
  the vcpkg submodule).

- Reasoning: a fixed checked-in set is simpler (no auth, no network failure
  modes, hermetic/offline builds, reproducible validation), and matches the
  ecosystem norm for coping with AUTOSAR distribution constraints. Empirical
  nuance found while vendoring: the login wall guards the full release
  *packages*, while the standalone `AUTOSAR_MMOD_XMLSchema.zip` archives for
  every release in our range (4.2.2–R21-11) are publicly reachable without an
  account — so "no public download link" is true for packages but not quite
  for schemas. This does not change the decision: fetching at runtime would
  still couple every validation run to autosar.org availability and to URL
  layouts that already differ per era (`CP/` vs `FO/` vs combined
  `R18-10_R4.4.0_R1.5.0/` paths — see `resources/schemas/README.md`).

- Consequences: the Schema Registry resolves exclusively from
  `resources/schemas/` (plus its on-disk cache); adding a release means
  vendoring its XSD and re-running configure, never adding download code.
  The checked-in XSDs are AUTOSAR-copyrighted: redistribution terms must be
  verified before any public open-source release (see the TODO in
  `resources/schemas/README.md`), with the fallback being an authenticated
  download-at-setup script plus hash verification.

- Amendment — user-supplied schemas (2026-09-19, supersedes the checked-in
  half above): AUTOSAR's legal terms state that beyond personal informational
  use, "no part of the work may be utilized or reproduced, in any form or by
  any means, without permission in writing from the publisher" — the same
  restriction appears in the `_disclaimer.txt` of every schema archive we
  downloaded. Redistributing raw XSDs in a public repo is unauthorized
  reproduction, a bar this project cannot clear on its own. So:
  - `resources/schemas/*.xsd` is gitignored; the repo tracks only the
    directory layout, the README (lookup contract + obtain steps), and the
    CMake wiring. The seven XSDs obtained during development stay on local
    disks only.
  - Resolution contract going forward: `--xsd-path <dir>` > 
    `$PARSEX_SCHEMA_DIR` > built-in default
    (`<prefix>/share/parsex/schemas`, build-tree fallback). Each user
    downloads the archives onto their own machine; the Schema Registry never
    downloads anything itself (that half of the original decision is
    reaffirmed — grep still shows no fetch code in first-party sources).
  - Pre-compiling schema data into committed generated code (Strategy 2, cf.
    `autosar-data-specification`) stays a possible future step but needs its
    own legal review first; it is explicitly *not* adopted by this amendment.
