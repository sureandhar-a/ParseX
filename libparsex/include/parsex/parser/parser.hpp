#pragma once

#include <filesystem>

#include <parsex/model/parsed_file.hpp>

// Stateless Parser: no shared mutable state across calls, so parseFile() is
// const and a default-constructed Parser{} is the normal entry point.
//
// parseFile() wires the pipeline for one ARXML file: Loader (raw tree) ->
// release detection (schema filename -> release string, shared with the
// Schema Registry) -> Model Builder (typed objects + warnings) -> ParsedFile.
// Best-effort throughout: absent schema-optional content becomes warnings,
// never hard errors (only Validator::validateSchema() fails conformance).
//
// Failures that DO throw: unreadable/ill-formed input (std::runtime_error),
// a root element with no xsi:schemaLocation (std::runtime_error —
// interim; the error-handling PBI owns the final taxonomy), and an
// unrecognized schema filename (UnsupportedReleaseError).
class Parser {
public:
    Parser() = default;
    ParsedFile parseFile(const std::filesystem::path& path) const;
};
