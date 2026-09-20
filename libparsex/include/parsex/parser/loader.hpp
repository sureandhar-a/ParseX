#pragma once

#include <filesystem>

#include <parsex/raw/raw_document.hpp>

// Single-pass Span-Tracking Loader: builds the raw node tree (RawNode tag
// name, attributes, byte span, parent/child links) from one XML file.
//
// Mechanism: SAX2 start/end-element callbacks drive the walk in document
// order, and a small width-aware raw-byte scanner locates each element's
// span boundaries (`<tag ...>` / `</tag>`) from the file bytes, skipping
// text, comments, PIs, CDATA sections, and declarations. No libxml2 position
// API is used anywhere: the byte-offset spike first showed the reader loop's
// xmlTextReaderByteConsumed() reports input-buffer fill rather than node
// positions, and re-measuring against the linked libxml2 2.15.3 showed the
// in-callback replacement counts transcoded (UTF-8) units instead of raw
// file bytes for non-UTF-8 input — so any libxml2 offset is version-coupled.
// Scanning raw bytes is exact on every version and encoding (see
// docs/decisions/0003-byte-offset-tracking.md, including its 2.15.3
// amendment, and spikes/byte_offset_NOTES.md).
//
// The scanner asserts every expected tag name as it goes: SAX events arrive
// in document order, so any mismatch means scanner/parser divergence, which
// throws loudly rather than recording a wrong span.
//
// No domain-object interpretation happens here (that is the Protocol Model
// Builder's job): the tree is faithful to any XML shape, not just ARXML's
// CAN elements. Element text content is dropped — RawNode has no text field
// by design.
//
// Hardening matches the secure-reader setup (see parser/secure_reader.hpp):
// XML_PARSE_NONET, and no XML_PARSE_NOENT / XML_PARSE_DTDLOAD, so external
// entities stay unresolved.
//
// Hard failures (unreadable file, no document element, ill-formed XML) throw
// std::runtime_error for now; the error-handling PBI owns the final failure
// taxonomy (including the XXE-rejection test that builds on this setup).
RawDocument loadRawDocument(const std::filesystem::path& path);
