#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

// Reusable JSON Schema conformance check (PAR-175).
//
// Loads the schema file at schemaPath and validates document against it with
// json-schema-validator, following relative $refs (e.g. the envelope schema's
// "validation_result.schema.json" / "diff_report.schema.json") from the
// schema file's own parent directory.
//
// Returns true when the document validates cleanly. On failure returns false
// and — when errorOut is non-null — fills it with the validator's own error
// details so a failing test's output is diagnostic, not a bare boolean.
// A missing/unparseable schema file also returns false with details.
namespace parsex::json_contract {

[[nodiscard]] bool validatesAgainstSchema(const nlohmann::json& document,
                                          const std::filesystem::path& schemaPath,
                                          std::string* errorOut = nullptr);

}  // namespace parsex::json_contract
