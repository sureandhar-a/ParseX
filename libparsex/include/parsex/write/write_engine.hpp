#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include <parsex/model/parsed_project.hpp>
#include <parsex/validator/validation_result.hpp>

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

// Structured write error (PAR-158): reuses the Validator's ValidationError /
// ValidationResult convention (PAR-98) instead of inventing a new error type.
struct WriteError : std::runtime_error {
    ValidationResult result;

    explicit WriteError(ValidationResult errors);
};

class WriteEngine {
public:
    WriteEngine() = default;

    // Fail-fast presence checks (PAR-158): every element's required fields
    // (SHORT-NAME — schema-mandated on every element) are verified before any
    // file-system write occurs, so a failed write() never leaves a
    // partially-written file (the tree is fully in memory first, then
    // written to a temp path and renamed into place on success).
    [[nodiscard]] ValidationResult validate(const ParsedProject& project) const;

    void write(const ParsedProject& project, const std::filesystem::path& outputPath) const;
};
