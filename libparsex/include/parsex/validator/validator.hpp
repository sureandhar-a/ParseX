#pragma once

#include <libxml/xmlschemas.h>

#include <map>
#include <string>

#include <parsex/model/parsed_file.hpp>
#include <parsex/model/parsed_project.hpp>
#include <parsex/validator/validation_result.hpp>

// Public Validator facade (grown PBI by PBI; PBI 5 documents the full set).
//
// Thread-safety: the shared xmlSchema* may be read concurrently, but each
// validateSchema() call mints its own xmlSchemaValidCtxt* — never shared
// across calls or threads (libxml2 maintainer requirement).
class Validator {
public:
    Validator() = default;

    // PBI 1: full XSD validation of an already-parsed file against the shared
    // schema handle from the Schema Registry. Location attribution is wired
    // in by PAR-100; this step captures message/severity via structured
    // errors.
    //
    // Note: RawDocument currently owns no live xmlDoc (the Span-Tracking
    // Loader discards the SAX parse's doc and keeps only the RawNode tree),
    // so validation re-parses ParsedFile::sourcePath from disk into a
    // throwaway xmlDoc rather than trusting a null nativeHandle().
    ValidationResult validateSchema(const ParsedFile& file,
                                    ::xmlSchema* sharedSchema) const;

    // PBI 2: short-name package-path index over the raw tree, built once per
    // project. Maps absolute AUTOSAR paths ("/Pkg/Sub/Element") to the node
    // defining them. Built generically over RawNode (not just the six typed
    // CAN types) since REF targets live all over the schema.
    std::map<std::string, const RawNode*> buildReferencePathIndex(
        const ParsedProject& project) const;

    // PBI 2: resolve every XXX-REF against the index; dangling paths and
    // DEST mismatches become errors (implemented PAR-103/PAR-104).
    ValidationResult validateReferences(const ParsedProject& project) const;

private:
    static void onSchemaError(void* userData, const xmlError* error);
};
