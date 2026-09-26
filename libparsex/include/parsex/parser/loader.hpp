#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

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
// Hard failures throw ParseError (see parser/parse_error.hpp): unreadable
// files as ParseErrorReason::Io (checked before any libxml2 context exists),
// malformed XML as ParseErrorReason::Syntax with libxml2's own message plus
// line/column folded in (captured via a context error handler, so nothing
// leaks to stderr).
RawDocument loadRawDocument(const std::filesystem::path& path);

// In-memory variant for fuzzing and tests: parses bytes already in memory
// without touching the filesystem. displayPath is diagnostic-only (error
// messages, telemetry labels) — no file is opened. Throws the same
// ParseError[Syntax] / runtime_error set as loadRawDocument() for malformed
// input, so fuzz harnesses can treat any exception as "handled gracefully".
RawDocument loadRawDocumentFromMemory(std::span<const std::uint8_t> input,
                                      const std::filesystem::path& displayPath);
