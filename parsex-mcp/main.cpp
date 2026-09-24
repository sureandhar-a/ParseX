#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "dispatch.hpp"
#include "protocol.hpp"
#include "server.hpp"
#include "tools.hpp"
#include "transport.hpp"

// Convention: writeMessage() is the only function allowed to write to
// std::cout. All diagnostics go to stderr so stdout stays pure protocol.

namespace {

// Routes one validated message. Discovery is optional: every method works
// whether or not the client asked for identity first. Version is checked
// once here so every method shares the same path.
void dispatchMessage(const nlohmann::json& message, const ToolRegistry& registry) {
    try {
        if (!message.is_object()) {
            return;
        }
        // Notifications without id get no response, but version still applies
        // when present. Use null id for error paths that lack one.
        const bool hasId = message.contains("id");
        const nlohmann::json id = hasId ? message["id"] : nlohmann::json(nullptr);
        // Stateless version check on every request.
        try {
            auto version = extractProtocolVersion(message);
            if (version.has_value() && !isSupportedVersion(*version)) {
                if (!hasId) {
                    return;
                }
                nlohmann::json response;
                response["jsonrpc"] = "2.0";
                response["id"] = id;
                response["error"] = {{"code", -32600},
                                     {"message",
                                      "Unsupported protocol version: " + *version +
                                          ". Supported versions: 2026-07-28"},
                                     {"data",
                                      {{"supportedVersions",
                                        nlohmann::json::array({"2026-07-28"})}}}};
                writeMessage(response);
                return;
            }
        } catch (...) {
        }
        if (!hasId) {
            // Legacy handshake uses a request with id, so notifications fall
            // through here with no response.
            try {
                std::string methodCheck;
                if (message.contains("method") && message["method"].is_string()) {
                    methodCheck = message["method"].get<std::string>();
                }
                if (methodCheck == "initialize") {
                    return;
                }
            } catch (...) {
            }
            return;
        }
        std::string method;
        nlohmann::json params = nlohmann::json::object();
        try {
            if (message.contains("method") && message["method"].is_string()) {
                method = message["method"].get<std::string>();
            }
            if (message.contains("params") && message["params"].is_object()) {
                params = message["params"];
            }
        } catch (...) {
        }
        if (method == "server/discover") {
            nlohmann::json response;
            response["jsonrpc"] = "2.0";
            response["id"] = id;
            response["result"] = buildDiscoverResult();
            writeMessage(response);
            return;
        }
        if (method == "tools/list") {
            std::string cursor;
            try {
                if (params.contains("cursor") && params["cursor"].is_string()) {
                    cursor = params["cursor"].get<std::string>();
                }
            } catch (...) {
            }
            nlohmann::json response;
            response["jsonrpc"] = "2.0";
            response["id"] = id;
            response["result"] = buildToolsListResult(registry, cursor);
            writeMessage(response);
            return;
        }
        if (method == "tools/call") {
            try {
                writeMessage(dispatchToolsCall(id, params, registry));
            } catch (...) {
            }
            return;
        }
        if (method == "initialize") {
            nlohmann::json response;
            response["jsonrpc"] = "2.0";
            response["id"] = id;
            response["error"] = {
                {"code", -32600},
                {"message",
                 "Legacy initialize handshake is not supported. This server requires protocol "
                 "version 2026-07-28 with per-request _meta."},
                {"data", {{"supportedVersions", nlohmann::json::array({"2026-07-28"})}}}};
            writeMessage(response);
            return;
        }
        writeMessage(makeMethodNotFound(id));
    } catch (...) {
    }
}

}  // namespace

int main() {
    ToolRegistry registry = ToolRegistry::withSchemas();
    registry.setHandler("parse_arxml", parseTool);
    registry.setHandler("validate_arxml", validateTool);
    registry.setHandler("diff_arxml", diffTool);
    registry.setHandler("write_arxml", writeTool);
    std::string line;
    while (std::getline(std::cin, line)) {
        // Empty lines carry no message; skip without responding.
        if (line.empty()) {
            continue;
        }
        nlohmann::json message;
        try {
            message = nlohmann::json::parse(line);
        } catch (const std::exception& ex) {
            // No valid JSON means no reliable id. Try to recover one for a
            // proper Parse Error; otherwise log to stderr and keep looping.
            try {
                auto recovered = tryRecoverId(line);
                if (recovered.has_value()) {
                    try {
                        writeMessage(makeParseError(*recovered));
                    } catch (...) {
                    }
                } else {
                    std::cerr << "skipping malformed line: " << ex.what() << "\n";
                }
            } catch (...) {
                std::cerr << "skipping malformed line\n";
            }
            continue;
        }
        try {
            if (!isValidEnvelope(message)) {
                try {
                    writeMessage(makeInvalidRequest(idOrNull(message)));
                } catch (...) {
                }
                continue;
            }
            dispatchMessage(message, registry);
        } catch (...) {
            // Validation and dispatch must never escape; the loop survives
            // any sequence of malformed lines.
            std::cerr << "skipping invalid message\n";
        }
    }
    // EOF is the normal shutdown path for a stdio server.
    return 0;
}
