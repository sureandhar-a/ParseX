#pragma once

#include <string_view>

// Single project-wide JSON contract version (PAR-170).
//
// Every JSON-emitting code path (Validator, Diff Engine, later CLI / MCP Server)
// must reference kContractVersion rather than a hardcoded literal, so a version
// bump is a one-line change.
//
// Semver bump rules (see schemas/VERSIONING.md):
//   major = breaking change (add/remove required field, narrow enum, change type, ...)
//   minor = additive / backward-compatible (new optional field, new `kind` value)
//   patch = documentation-only or non-schema-affecting fix.
// Starts at "1.0.0": there is no prior public JSON contract to be compatible with.
namespace parsex::json_contract {

inline constexpr std::string_view kContractVersion = "1.0.0";

}  // namespace parsex::json_contract
