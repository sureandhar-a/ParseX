#pragma once

#include <libxml/xmlschemas.h>

#include <parsex/model/parsed_file.hpp>
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

private:
    static void onSchemaError(void* userData, const xmlError* error);
};
