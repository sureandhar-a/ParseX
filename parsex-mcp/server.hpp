#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <parsex/version.hpp>

#include "tool_registry.hpp"

// Identity and capability surface for the local bridge.
//
// Single place for the protocol version this server speaks and the
// discovery payload every client may request first (but is never required
// to request first).

inline constexpr std::string_view kProtocolVersion = "2026-07-28";
inline constexpr std::string_view kServerName = "parsex-mcp";

inline std::vector<std::string> supportedVersions() {
    return {std::string(kProtocolVersion)};
}

inline nlohmann::json buildDiscoverResult() {
    nlohmann::json result;
    result["resultType"] = "complete";
    result["supportedVersions"] = nlohmann::json::array({std::string(kProtocolVersion)});
    result["capabilities"] = {{"tools", {{"listChanged", false}}}};
    result["_meta"] = {{"io.modelcontextprotocol/serverInfo",
                        {{"name", std::string(kServerName)},
                         {"version", std::string(libparsexVersion())}}}};
    return result;
}

inline nlohmann::json toolToWire(const ToolDefinition& tool) {
    nlohmann::json wire;
    wire["name"] = tool.name;
    wire["title"] = tool.title;
    wire["description"] = tool.description;
    wire["inputSchema"] = tool.inputSchema;
    if (tool.outputSchema.has_value()) {
        wire["outputSchema"] = *tool.outputSchema;
    }
    wire["annotations"] = tool.annotations;
    return wire;
}

// Lists tools in stable registration order. Accepts an optional cursor for
// pagination but always returns a single page (four tools fit easily).
inline nlohmann::json buildToolsListResult(const ToolRegistry& registry,
                                           const std::string& /*cursor*/ = "") {
    nlohmann::json result;
    result["tools"] = nlohmann::json::array();
    for (const ToolDefinition* tool : registry.ordered()) {
        result["tools"].push_back(toolToWire(*tool));
    }
    return result;
}
