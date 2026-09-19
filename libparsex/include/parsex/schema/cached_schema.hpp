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
// about which one frees it last. The shared_ptr carries XmlSchemaDeleter,
// so the last holder frees via xmlSchemaFree (see resolveSchema()).
//
// Thread-safety contract on the shared handle:
// - The xmlSchema* itself (the parsed schema definition) is safe to read
//   from multiple call sites, including "at the same time" in a future
//   multi-threaded scenario, because nothing mutates it after xmlSchemaParse
//   returns.
// - An xmlSchemaValidCtxt* (a validation context created from that schema)
//   is NOT shareable: each caller (Parser, Validator, or two concurrent
//   calls to either) must create its own via xmlSchemaNewValidCtxt(schema)
//   and must not use it from more than one thread. (libxml2 is not
//   thread-safe per-context: https://mail.gnome.org/archives/xml/2012-January/msg00030.html)
// - Practical consequence today: the CLI is single-threaded and sequential
//   (parse-then-validate-then-write, never concurrent), so this contract is
//   automatically satisfied with no locking. If a future Feature ever
//   parallelizes file processing, each worker must create its own
//   valid-context from the shared schema rather than sharing one.
//
// Design-doc pointer: docs/lld.md's Schema Registry section should
// cross-reference this contract ("see cached_schema.hpp's doc comment for
// the thread-safety contract on the shared handle") — lld.md does not exist
// yet (2026-09-19), so that one-liner is still owed when it is written.
struct CachedSchema {
    std::string autosarRelease;
    std::filesystem::path sourceXsdPath;    // the user-supplied .xsd this came from
    std::shared_ptr<xmlSchema> schemaHandle;
};
