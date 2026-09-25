# Pre-release checklist

Copy-paste this into the release-tracking issue and work it top to bottom.
Every box must be genuinely satisfied, not skipped — the dry-run release
proved each one matters.

- [ ] Continuous integration green on the release branch (build plus unit
  checks, shared output checks, sanitizer suite, coverage floor, lint).
- [ ] Coverage floor holds (`python3 scripts/check_coverage_floor.py`,
  currently 35%).
- [ ] `CHANGELOG.md` `## [Unreleased]` reviewed and non-empty for a
  meaningful release (manual judgment: entries describe user-facing change).
- [ ] Version bumped via `docs/versioning.md` (single source, changelog
  moved to a dated section, packaging manifests match).
- [ ] Design docs spot-checked against the release's actual changes
  (`docs/spec.md`, `docs/hld.md`, `docs/lld.md`) (manual judgment:
  no stale flags, tools, or field names).
- [ ] Packaging port version matches (`ports/libparsex/vcpkg.json`
  `"version"` equals the bumped version; portfile `REF` and `SHA512`
  pinned to the tag once cut — see below).
- [ ] Release notes drafted from the changelog dated section plus
  highlights (see the release template).
- [ ] Tag `vX.Y.Z` (annotated) pushed; release automation green; artifacts
  and notes verified on the published release.

## Automated vs. manual

Automated now: integration pipeline, coverage floor, manifest version match
(checkable by script), automation trigger on tag.

Manual judgment for v1: changelog completeness, doc accuracy, release-notes
highlights. Automating those checks is future work, not this pass.

## Port hash pinning

After pushing the tag, download the source archive
(`https://github.com/sureandhar-a/ParseX/archive/vX.Y.Z.tar.gz`), compute
its `SHA512`, and record it in `ports/libparsex/portfile.cmake`, replacing
the placeholder. Re-run the manual port validation in
`docs/vcpkg-usage.md` before announcing the release.
