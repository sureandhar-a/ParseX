#pragma once

#include <memory>

#include <libxml/xmlschemas.h>

// RAII wrappers for libxml2's schema-related handles, following the same
// unique_ptr + custom deleter pattern as XmlDocPtr (see raw/raw_document.hpp).
//
// Each schema-related type has exactly one matching free function — no
// ref-counting or shared-teardown gotchas.
//
// Thread-safety note (see docs; libxml2 xml list, Jan 2012): libxml2 is not
// thread-safe per-context. A given xmlSchema*/xmlSchemaValidCtxt* must not be
// used concurrently from multiple threads without external locking. The CLI
// is single-threaded, so the shared schema handle is fine — but each caller
// must still create its OWN validation context rather than sharing one.

struct XmlSchemaParserCtxtDeleter {
    void operator()(xmlSchemaParserCtxt* ctxt) const noexcept {
        if (ctxt) xmlSchemaFreeParserCtxt(ctxt);
    }
};
using XmlSchemaParserCtxtPtr = std::unique_ptr<xmlSchemaParserCtxt, XmlSchemaParserCtxtDeleter>;

struct XmlSchemaDeleter {
    void operator()(xmlSchema* schema) const noexcept {
        if (schema) xmlSchemaFree(schema);
    }
};
using XmlSchemaPtr = std::unique_ptr<xmlSchema, XmlSchemaDeleter>;

struct XmlSchemaValidCtxtDeleter {
    void operator()(xmlSchemaValidCtxt* ctxt) const noexcept {
        if (ctxt) xmlSchemaFreeValidCtxt(ctxt);
    }
};
using XmlSchemaValidCtxtPtr = std::unique_ptr<xmlSchemaValidCtxt, XmlSchemaValidCtxtDeleter>;
