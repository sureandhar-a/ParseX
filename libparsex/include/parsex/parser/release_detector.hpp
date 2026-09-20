#pragma once

#include <optional>
#include <string>

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
