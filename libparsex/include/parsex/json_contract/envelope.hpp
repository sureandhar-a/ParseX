#pragma once

#include <string_view>

#include <nlohmann/json.hpp>

// Shared envelope builder (PAR-176).
//
// Every JSON-emitting Feature (Validator, Diff Engine, later CLI / MCP Server)
// calls wrapEnvelope() instead of hand-rolling the $schema/contractVersion/
// toolVersion/kind/payload wrapping per call site, so the envelope shape has
// exactly one construction site.
namespace parsex::json_contract {

// Builds {"$schema", "contractVersion" (= kContractVersion), "toolVersion"
// (= libparsexVersion()), "kind", "payload"} around the given payload.
[[nodiscard]] nlohmann::json wrapEnvelope(std::string_view kind, nlohmann::json payload);

}  // namespace parsex::json_contract
