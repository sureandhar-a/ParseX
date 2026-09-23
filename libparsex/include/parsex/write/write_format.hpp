#pragma once

#include <libxml/tree.h>

#include <string>

// Whitespace guard + file writer (PAR-151/152).
//
// Audit (PAR-151, step 1): every xmlAddChild/xmlNewChild call across
// write_context.cpp and write_elements.cpp was reviewed — none inserts
// whitespace-only text nodes between elements. Trees are built exclusively
// via xmlNewNode/xmlNewChild with nullptr content plus xmlNodeSetContent with
// real values (SHORT-NAME, LENGTH, REF paths, ...); no development-spacing
// text nodes exist. The lxml FAQ / 2001 mailing-list / PostgreSQL gotcha
// therefore cannot trigger unless a future change adds one — which the guard
// below catches automatically.

// Throws std::runtime_error when the tree contains any whitespace-only
// XML_TEXT_NODE. Use as a standing guard in tests, not a one-time check.
void assertNoWhitespaceOnlyTextNodes(const xmlNode* root);

// Writes the finalized tree to disk (PAR-152) via xmlSaveFormatFileEnc with
// format=1 (indentation). Returns bytes written; throws std::runtime_error
// with a clear message on failure (bad path, unwritable location).
// The XML declaration (<?xml version="1.0" encoding="UTF-8"?>) is emitted
// automatically as the first line since docs are created via
// xmlNewDoc(BAD_CAST "1.0") — verified by test, not assumed.
int writeXmlToFile(xmlDocPtr doc, const std::string& path);
