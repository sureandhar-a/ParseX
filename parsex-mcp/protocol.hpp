#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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

inline nlohmann::json makeMethodNotFound(const nlohmann::json& id) {
    return makeProtocolError(id, -32601, "Method not found");
}

// Upper bound on JSON nesting accepted from the transport. MCP requests are
// shallow (tool arguments are flat objects), so the cap is generous. It exists
// because nlohmann's parser is iterative but copying, dumping, and schema-
// validating a parsed value all recurse once per level: a single ~200 KB line
// nested ~100k levels deep otherwise overflows the stack and kills the server.
inline constexpr std::size_t kMaxJsonDepth = 128;

// True when `line` opens more than `maxDepth` nested arrays/objects at any
// point. Linear and allocation-free, so it runs before parsing and an over-deep
// line is never materialised. Brackets inside string literals (including
// escaped quotes) are ignored. Malformed input is fine: this only bounds depth,
// the real parser still decides validity.
inline bool exceedsMaxDepth(std::string_view line, std::size_t maxDepth = kMaxJsonDepth) noexcept {
    std::size_t depth = 0;
    bool inString = false;
    bool escaped = false;
    for (const char chr : line) {
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (chr == '\\') {
                escaped = true;
            } else if (chr == '"') {
                inString = false;
            }
            continue;
        }
        if (chr == '"') {
            inString = true;
        } else if (chr == '[' || chr == '{') {
            if (++depth > maxDepth) {
                return true;
            }
        } else if ((chr == ']' || chr == '}') && depth > 0) {
            --depth;
        }
    }
    return false;
}

inline nlohmann::json makeNestingTooDeep(const nlohmann::json& id) {
    nlohmann::json response = makeProtocolError(
        id, -32600, "Invalid Request: JSON nesting exceeds the maximum depth of " + std::to_string(kMaxJsonDepth));
    response["error"]["data"] = {{"maxDepth", kMaxJsonDepth}};
    return response;
}

namespace protocol_detail {

inline bool isJsonSpace(char chr) noexcept {
    return chr == ' ' || chr == '\t' || chr == '\n' || chr == '\r' || chr == '\f' || chr == '\v';
}

inline bool isAsciiDigit(char chr) noexcept {
    return chr >= '0' && chr <= '9';
}

inline std::size_t skipJsonSpace(const std::string& line, std::size_t pos) noexcept {
    while (pos < line.size() && isJsonSpace(line[pos])) {
        ++pos;
    }
    return pos;
}

// Outcome of reading an id value at one `"id"` occurrence: a value, a hard
// "no usable id" (number out of range, as before), or "try the next one".
enum class IdScan : std::uint8_t { Found, GiveUp, KeepLooking };

// Reads `: <number|string|null>` starting right after an `"id"` key.
inline IdScan readIdValue(const std::string& line, std::size_t pos, nlohmann::json& out) {
    pos = skipJsonSpace(line, pos);
    if (pos >= line.size() || line[pos] != ':') {
        return IdScan::KeepLooking;
    }
    pos = skipJsonSpace(line, pos + 1);
    if (pos >= line.size()) {
        return IdScan::KeepLooking;
    }
    if (line.compare(pos, 4, "null") == 0) {
        out = nullptr;
        return IdScan::Found;
    }
    if (line[pos] == '"') {
        const std::size_t close = line.find('"', pos + 1);
        if (close == std::string::npos) {
            return IdScan::KeepLooking;
        }
        out = line.substr(pos + 1, close - pos - 1);
        return IdScan::Found;
    }
    std::size_t end = (line[pos] == '-') ? pos + 1 : pos;
    const std::size_t digitsStart = end;
    while (end < line.size() && isAsciiDigit(line[end])) {
        ++end;
    }
    if (end == digitsStart) {
        return IdScan::KeepLooking;
    }
    try {
        out = std::stoll(line.substr(pos, end - pos));
        return IdScan::Found;
    } catch (...) {
        return IdScan::GiveUp;
    }
}

} // namespace protocol_detail

// Best-effort id recovery from a line that failed to parse as JSON.
// Returns nullopt when no usable id is present.
//
// Finds the leftmost `"id"` key followed by `:` and a number, a string, or
// null (whitespace allowed around the colon) — the same match the earlier
// std::regex produced. A hand-written linear scan instead of the regex:
// libstdc++ evaluates `"[^"]*"` recursively, so a ~100 KB unterminated string
// after `"id":` overflowed the stack on the very path meant to survive
// malformed input.
inline std::optional<nlohmann::json> tryRecoverId(const std::string& line) {
    try {
        constexpr std::string_view kKey = "\"id\"";
        for (std::size_t from = line.find(kKey); from != std::string::npos; from = line.find(kKey, from + 1)) {
            nlohmann::json value;
            switch (protocol_detail::readIdValue(line, from + kKey.size(), value)) {
            case protocol_detail::IdScan::Found:
                return value;
            case protocol_detail::IdScan::GiveUp:
                return std::nullopt;
            case protocol_detail::IdScan::KeepLooking:
                break;
            }
        }
        return std::nullopt;
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
