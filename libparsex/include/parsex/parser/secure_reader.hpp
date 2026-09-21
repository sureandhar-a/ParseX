#pragma once

#include <filesystem>
#include <memory>

#include <libxml/xmlreader.h>

// Hardened libxml2 reader setup for the Loader: every reader ParseX opens goes
// through openSecureReader(), so external-entity handling is explicit in our
// own code instead of relying on libxml2's defaults. Follows the same
// unique_ptr + custom deleter pattern as XmlDocPtr (see raw/raw_document.hpp)
// and the schema handles (see schema/xml_schema_raii.hpp).

struct XmlReaderDeleter {
    void operator()(xmlTextReaderPtr reader) const noexcept {
        if (reader != nullptr) {
            xmlFreeTextReader(reader);
        }
    }
};
using XmlReaderPtr = std::unique_ptr<xmlTextReader, XmlReaderDeleter>;

// Parse options for every reader the Loader opens.
//
// Set: XML_PARSE_NONET — no network access of any kind (DTD, entities,
// schema references), even for URLs that are not technically entities.
//
// Deliberately absent: XML_PARSE_NOENT (would substitute entities, including
// external ones) and XML_PARSE_DTDLOAD (would load external DTD subsets).
// Passing neither is what keeps external entities unresolved.
//
// Since libxml2 2.9 external-entity substitution is off by default, but that
// default has changed before and distro builds may differ — so ParseX pins the
// safe combination here. See the OWASP XML External Entity Prevention Cheat
// Sheet (libxml2 section) and docs/decisions/0003-byte-offset-tracking.md.
inline constexpr int kSecureReaderOptions = XML_PARSE_NONET;

static_assert((kSecureReaderOptions & XML_PARSE_NONET) != 0,
              "network access must stay disabled on the Loader reader");
static_assert((kSecureReaderOptions & (XML_PARSE_NOENT | XML_PARSE_DTDLOAD)) == 0,
              "entity substitution / external DTD loading must stay disabled");

// Opens path with kSecureReaderOptions and encoding auto-detection.
// Returns nullptr when the file cannot be opened; the caller reports the
// error (the error-handling PBI owns the failure taxonomy, including the
// XXE-rejection test that builds on this setup).
XmlReaderPtr openSecureReader(const std::filesystem::path& path);
