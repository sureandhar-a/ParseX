#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <parsex/version.hpp>

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
