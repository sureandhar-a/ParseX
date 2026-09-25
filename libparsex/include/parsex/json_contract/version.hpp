#pragma once

#include <string_view>

#include <parsex/version_config.hpp>

// Single project-wide JSON contract version, derived from the same
// top-level CMake project VERSION as the tool version (see
// parsex/version_config.hpp) — one bump updates both together for v1.
//
// Every JSON-emitting code path (Validator, Diff Engine, later CLI / MCP Server)
// must reference kContractVersion rather than a hardcoded literal, so a version
// bump is a one-line change.
//
// Semver bump rules (see schemas/VERSIONING.md):
//   major = breaking change (add/remove required field, narrow enum, change type, ...)
//   minor = additive / backward-compatible (new optional field, new `kind` value)
//   patch = documentation-only or non-schema-affecting fix.
namespace parsex::json_contract {

inline constexpr std::string_view kContractVersion = PARSEX_VERSION;

}  // namespace parsex::json_contract
