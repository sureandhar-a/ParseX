#pragma once

#include <filesystem>
#include <vector>

#include <parsex/model/parsed_file.hpp>
#include <parsex/model/parsed_project.hpp>

// How parseProject() finds the files that make up a project. Only
// ExplicitList is implemented so far; DirectoryScan and LazyOnReference
// arrive in later subtasks.
enum class FileDiscoveryMode { ExplicitList, DirectoryScan, LazyOnReference };

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
    // Parses every file in entryPoints independently (via parseFile) and
    // collects the results into ParsedProject.files, in order. Cross-file
    // reference resolution is a later subtask — resolvedRefs stays empty.
    //
    // Whole-call failure (v1 default, documented choice): if any single file
    // fails, its exception propagates and the entire call fails. A project
    // with one unreadable file is a project you cannot fully trust, and
    // fail-fast matches CLI correctness requirements; partial results with
    // per-file errors remain a possible future relaxation.
    ParsedProject parseProject(const std::vector<std::filesystem::path>& entryPoints,
                               FileDiscoveryMode mode) const;
};
