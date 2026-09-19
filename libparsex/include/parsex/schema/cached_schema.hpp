#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <libxml/xmlschemas.h>

// One resolved AUTOSAR XSD schema, shared by Parser and Validator.
//
// schemaHandle is a shared_ptr (not unique_ptr) for the same reason as
// ParsedFile::rawDocument: both the Parser and the Validator hold a
// reference to the same resolved schema, and neither should have to worry
// about which one frees it last.
//
// NOTE: the custom deleter that makes this handle truly RAII (freeing via
// libxml2's xmlSchemaFree instead of `delete`) is wired in the next subtask;
// until then only placeholder (null) handles should be constructed.
struct CachedSchema {
    std::string autosarRelease;
    std::filesystem::path sourceXsdPath;    // the user-supplied .xsd this came from
    std::shared_ptr<xmlSchema> schemaHandle;
};
