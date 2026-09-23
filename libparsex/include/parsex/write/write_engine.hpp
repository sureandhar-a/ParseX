#pragma once

#include <filesystem>

#include <parsex/model/parsed_project.hpp>

// Public Write Engine facade (PAR-157; minimal assembly landed early to
// unblock PAR-154's round-trip helper — version wiring, structured errors,
// and sanitizer validation arrive in PAR-157/158/159).
//
// write() runs, in order: PAR-139's tree construction (root scaffold +
// per-type builders + nested collections) -> PAR-140's deterministic ordering
// pass (attribute sort + short-name sort) -> PAR-141's pretty-printed file
// write.
//
// Schema version: threaded from ParsedProject's files[0].autosarRelease
// (already tracked by the Parser per file — no new field needed); falls back
// to "4.4.0" for an empty project. Documented here per PAR-157 step 2.
class WriteEngine {
public:
    WriteEngine() = default;

    void write(const ParsedProject& project, const std::filesystem::path& outputPath) const;
};
