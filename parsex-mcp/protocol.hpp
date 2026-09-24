#pragma once

#include <optional>
#include <regex>
#include <string>

#include <nlohmann/json.hpp>

// JSON-RPC envelope checks shared by transport and dispatch.
//
// Never throws: all helpers catch internally so the read loop survives
// arbitrary byte sequences.

inline nlohmann::json makeProtocolError(const nlohmann::json& id, int code,
                                        const std::string& message) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = {{"code", code}, {"message", message}};
    return response;
}

inline nlohmann::json makeParseError(const nlohmann::json& id) {
    return makeProtocolError(id, -32700, "Parse error");
}

inline nlohmann::json makeInvalidRequest(const nlohmann::json& id) {
    return makeProtocolError(id, -32600, "Invalid Request");
}

// Best-effort id recovery from a line that failed to parse as JSON.
// Returns nullopt when no usable id is present.
inline std::optional<nlohmann::json> tryRecoverId(const std::string& line) {
    try {
        // Look for "id": <number|string|null> pattern.
        static const std::regex idPattern(R"("id"\s*:\s*(-?\d+|"[^"]*"|null))");
        std::smatch match;
        if (!std::regex_search(line, match, idPattern)) {
            return std::nullopt;
        }
        std::string token = match[1].str();
        if (token == "null") {
            return nlohmann::json(nullptr);
        }
        if (!token.empty() && token.front() == '"') {
            return nlohmann::json(token.substr(1, token.size() - 2));
        }
        try {
            return nlohmann::json(std::stoll(token));
        } catch (...) {
            return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
}

// Validates the JSON-RPC envelope. Returns true when the message may proceed
// to dispatch. Notifications (methods starting with "notifications/") may omit
// id; all other messages must carry one.
inline bool isValidEnvelope(const nlohmann::json& message) {
    try {
        if (!message.is_object()) {
            return false;
        }
        auto it = message.find("jsonrpc");
        if (it == message.end() || !it->is_string() || it->get<std::string>() != "2.0") {
            return false;
        }
        if (message.contains("id")) {
            return true;
        }
        // No id: only acceptable for notifications.
        if (message.contains("method") && message["method"].is_string()) {
            const std::string method = message["method"].get<std::string>();
            if (method.rfind("notifications/", 0) == 0) {
                return true;
            }
        }
        return false;
    } catch (...) {
        return false;
    }
}

inline nlohmann::json idOrNull(const nlohmann::json& message) {
    try {
        if (message.is_object() && message.contains("id")) {
            return message["id"];
        }
    } catch (...) {
    }
    return nlohmann::json(nullptr);
}
