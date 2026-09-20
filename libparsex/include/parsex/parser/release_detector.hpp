#pragma once

#include <optional>
#include <string>

#include <parsex/parser/release_error.hpp>
#include <parsex/raw/raw_document.hpp>

// AUTOSAR release detection: pulls the release-identifying schema filename
// out of the root <AUTOSAR> element's xsi:schemaLocation attribute.
//
// schemaLocation's value is a whitespace-separated pair,
// "<namespace-uri> <schema-filename>" (e.g. http://autosar.org/schema/r4.0
// AUTOSAR_00046.xsd, or the older version-in-name style
// AUTOSAR_4-2-2.xsd). Returns the filename (second token), or std::nullopt
// when the attribute is missing, empty, or holds no filename — the caller
// (parseFile/parseProject) turns that into the appropriate error. Never
// throws for malformed values; there is nothing here that can fail.
std::optional<std::string> detectSchemaFilename(const RawDocument& document);

// Maps a schema filename from xsi:schemaLocation to the release string shared
// with the Schema Registry (its vendored "<release>.xsd" keys), e.g.
// "AUTOSAR_4-2-2.xsd" -> "4.2.2".
//
// The table is small, explicit, and manually maintained on purpose: AUTOSAR
// has used both a version-in-name scheme (4-2-2, 4-3-0) and a numeric scheme
// (00044, 00046, 00048-00050) with gaps, so no rule derives one from the
// other — the same conclusion real tooling reached (cf.
// https://github.com/cogu/autosar/issues/28). Filenames below are AUTOSAR's
// own distribution names, verified against scripts/download_schemas.py; the
// table grows only when ParseX adds a release, which is rare.
//
// Throws UnsupportedReleaseError (carrying the filename) for anything outside
// the supported range 4.2.2-R21-11. Membership in this table IS the range
// check: it holds exactly the supported releases, so a hit is always in
// range and a miss always throws.
std::string resolveRelease(const std::string& schemaFilename);
