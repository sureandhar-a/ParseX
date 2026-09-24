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

std::string readMethod(const nlohmann::json& message) {
    try {
        if (message.contains("method") && message.at("method").is_string()) {
            return message.at("method").get<std::string>();
        }
    } catch (const std::exception& ex) {
        std::cerr << "skipping unreadable method: " << ex.what() << "\n";
    }
    return "";
}

nlohmann::json readParams(const nlohmann::json& message) {
    try {
        if (message.contains("params") && message.at("params").is_object()) {
            return message.at("params");
        }
    } catch (const std::exception& ex) {
        std::cerr << "skipping unreadable params: " << ex.what() << "\n";
    }
    return nlohmann::json::object();
}

void sendVersionMismatch(const nlohmann::json& requestId, const std::string& version) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["id"] = requestId;
    response["error"] = {{"code", -32600},
                         {"message",
                          "Unsupported protocol version: " + version +
                              ". Supported versions: 2026-07-28"},
                         {"data",
                          {{"supportedVersions", nlohmann::json::array({"2026-07-28"})}}}};
    writeMessage(response);
}

void sendLegacyInitialize(const nlohmann::json& requestId) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["id"] = requestId;
    response["error"] = {
        {"code", -32600},
        {"message",
         "Legacy initialize handshake is not supported. This server requires protocol "
         "version 2026-07-28 with per-request _meta."},
        {"data", {{"supportedVersions", nlohmann::json::array({"2026-07-28"})}}}};
    writeMessage(response);
}

// Returns true when the message was a notification handled without a reply.
bool handleNotification(const nlohmann::json& message) {
    const std::string method = readMethod(message);
    if (method == "notifications/cancelled") {
        // Cancellation is best-effort in this version: the server handles
        // one request at a time on a single thread, so there is no
        // in-flight work to interrupt. A notice for an already-completed
        // or unknown request is safely ignored. True mid-call
        // cancellation would need cooperatively cancellable engines, out
        // of scope for now. Never responds on stdout (it is a
        // notification, not a request).
        try {
            std::string ref = "unknown";
            const nlohmann::json params = readParams(message);
            if (params.contains("requestId")) {
                ref = params.at("requestId").dump();
            }
            std::cerr << "cancellation ignored for " << ref << "\n";
        } catch (const std::exception& ex) {
            std::cerr << "cancellation ignored (unreadable ref): " << ex.what() << "\n";
        }
        return true;
    }
    return false;
}

void routeRequest(const nlohmann::json& requestId, const std::string& method,
                  const nlohmann::json& params, const ToolRegistry& registry) {
    if (method == "server/discover") {
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = requestId;
        response["result"] = buildDiscoverResult();
        writeMessage(response);
        return;
    }
    if (method == "tools/list") {
        std::string cursor;
        try {
            if (params.contains("cursor") && params.at("cursor").is_string()) {
                cursor = params.at("cursor").get<std::string>();
            }
        } catch (const std::exception& ex) {
            std::cerr << "ignoring unreadable cursor: " << ex.what() << "\n";
        }
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = requestId;
        response["result"] = buildToolsListResult(registry, cursor);
        writeMessage(response);
        return;
    }
    if (method == "tools/call") {
        writeMessage(dispatchToolsCall(requestId, params, registry));
        return;
    }
    if (method == "initialize") {
        sendLegacyInitialize(requestId);
        return;
    }
    writeMessage(makeMethodNotFound(requestId));
}

// Routes one validated message. Discovery is optional: every method works
// whether or not the client asked for identity first. Version is checked
// once here so every method shares the same path.
void dispatchMessage(const nlohmann::json& message, const ToolRegistry& registry) {
    if (!message.is_object()) {
        return;
    }
    // Notifications without id get no response, but version still applies
    // when present.
    const bool hasId = message.contains("id");
    if (!hasId) {
        std::string method;
        try {
            method = readMethod(message);
        } catch (const std::exception& ex) {
            std::cerr << "skipping notification: " << ex.what() << "\n";
            return;
        }
        if (method == "initialize") {
            return;
        }
        if (!handleNotification(message)) {
            std::cerr << "skipping notification without id\n";
        }
        return;
    }
    nlohmann::json requestId;
    try {
        requestId = message.at("id");
    } catch (const std::exception& ex) {
        std::cerr << "skipping message with unreadable id: " << ex.what() << "\n";
        return;
    }
    // Stateless version check on every request.
    try {
        const auto version = extractProtocolVersion(message);
        if (version.has_value() && !isSupportedVersion(*version)) {
            sendVersionMismatch(requestId, *version);
            return;
        }
    } catch (const std::exception& ex) {
        std::cerr << "skipping version check: " << ex.what() << "\n";
    }
    const std::string method = readMethod(message);
    const nlohmann::json params = readParams(message);
    try {
        routeRequest(requestId, method, params, registry);
    } catch (const std::exception& ex) {
        std::cerr << "dispatch failed: " << ex.what() << "\n";
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
                const auto recovered = tryRecoverId(line);
                if (recovered.has_value()) {
                    try {
                        writeMessage(makeParseError(*recovered));
                    } catch (const std::exception& sendEx) {
                        std::cerr << "failed to send parse error: " << sendEx.what() << "\n";
                    }
                } else {
                    std::cerr << "skipping malformed line: " << ex.what() << "\n";
                }
            } catch (const std::exception& recoverEx) {
                std::cerr << "skipping malformed line (" << recoverEx.what() << ")\n";
            }
            continue;
        }
        try {
            if (!isValidEnvelope(message)) {
                try {
                    writeMessage(makeInvalidRequest(idOrNull(message)));
                } catch (const std::exception& sendEx) {
                    std::cerr << "failed to send request error: " << sendEx.what() << "\n";
                }
                continue;
            }
            dispatchMessage(message, registry);
        } catch (const std::exception& ex) {
            // Validation and dispatch must never escape; the loop survives
            // any sequence of malformed lines.
            std::cerr << "skipping invalid message: " << ex.what() << "\n";
        }
    }
    // EOF is the normal shutdown path for a stdio server.
    return 0;
}
