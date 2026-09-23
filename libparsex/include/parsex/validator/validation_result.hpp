#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <parsex/raw/raw_span.hpp>

// Shared result type for every Validator check (schema, references,
// uniqueness, CAN semantics) so validateAll() can merge outputs uniformly.
//
// ValidationError is intentionally a flat struct, not a class hierarchy per
// check type: merge logic stays trivial and every check's tests look alike.
// Extra context a specific check needs (offending path, expected/actual type)
// travels in small optional fields rather than a variant — enough for the
// four checks in this Feature without over-engineering.
enum class Severity { Error, Warning };

struct ValidationError {
    Severity severity = Severity::Error;
    std::string code;       // stable check id, e.g. "schema.invalid",
                            // "ref.dangling", "shortname.duplicate",
                            // "can.dlc_mismatch", "can.signal_overlap"
    std::string message;    // human-readable detail
    std::optional<RawSpan> location;  // nullopt when not attributable

    // Optional per-check context (stay flat; only set what the check needs).
    std::optional<std::string> path;          // REF path / parent path
    std::optional<std::string> expectedType;  // DEST expectation
    std::optional<std::string> actualType;    // resolved tag / second signal
};

struct ValidationResult {
    std::vector<ValidationError> errors;

    [[nodiscard]] bool empty() const noexcept { return errors.empty(); }
    [[nodiscard]] bool hasErrors() const noexcept;
    [[nodiscard]] bool hasWarnings() const noexcept;
    void merge(const ValidationResult& other);

    // Structured JSON rendering (PAR-174): envelope-wrapped
    // ($schema/contractVersion/toolVersion/kind/payload) per
    // schemas/envelope.schema.json with kind "validationResult".
    // Optional error context (location/path/expectedType/actualType) is
    // omitted when absent, per schemas/CONVENTIONS.md.
    [[nodiscard]] nlohmann::json toJson() const;
    // Round-trip support for tests. Accepts either the full envelope (as
    // produced by toJson) or a bare payload object.
    [[nodiscard]] static ValidationResult fromJson(const nlohmann::json& jsonDoc);
};
