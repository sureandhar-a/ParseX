#pragma once

#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>

#include "protocol.hpp"
#include "tool_registry.hpp"

// Shared calling plumbing every tool plugs into.
//
// Two-tier errors:
// - Protocol errors (unknown tool, bad arguments) become JSON-RPC errors.
// - Engine failures become tool results with isError:true so the caller can
//   see what went wrong and retry.

inline bool argumentsMatchSchema(const nlohmann::json& arguments, const nlohmann::json& schema,
                                 std::string* errorOut = nullptr) {
    try {
        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(schema);
        nlohmann::json_schema::basic_error_handler handler;
        validator.validate(arguments, handler);
        if (!handler) {
            return true;
        }
        if (errorOut != nullptr) {
            *errorOut = "arguments do not match tool schema";
        }
        return false;
    } catch (const std::exception& ex) {
        if (errorOut != nullptr) {
            *errorOut = ex.what();
        }
        return false;
    }
}

inline nlohmann::json makeUnknownToolError(const nlohmann::json& id, const std::string& name) {
    return makeProtocolError(id, -32602, "Unknown tool: " + name);
}

inline nlohmann::json makeInvalidParamsError(const nlohmann::json& id, const std::string& detail) {
    return makeProtocolError(id, -32602, "Invalid params: " + detail);
}

// Human wording for engine failures follows the same exception-to-category
// thinking as the command-line surface (usage vs data vs io vs internal),
// without reusing process exit codes (there is no process exit per call).
inline std::string toolErrorText(const std::exception& ex) {
    try {
        return std::string(ex.what());
    } catch (...) {
        return "tool failed";
    }
}

inline nlohmann::json wrapToolSuccess(const nlohmann::json& structured, const std::string& summary) {
    nlohmann::json result;
    result["resultType"] = "complete";
    result["content"] = nlohmann::json::array({{{"type", "text"}, {"text", summary}}});
    result["structuredContent"] = structured;
    return result;
}

inline nlohmann::json wrapToolError(const std::string& message) {
    nlohmann::json result;
    result["content"] = nlohmann::json::array({{{"type", "text"}, {"text", message}}});
    result["isError"] = true;
    return result;
}

// Dispatches one tools/call request. Returns a JSON-RPC response object.
inline nlohmann::json dispatchToolsCall(const nlohmann::json& id, const nlohmann::json& params,
                                        const ToolRegistry& registry) {
    std::string name;
    nlohmann::json args = nlohmann::json::object();
    try {
        if (params.contains("name") && params["name"].is_string()) {
            name = params["name"].get<std::string>();
        }
        if (params.contains("arguments") && params["arguments"].is_object()) {
            args = params["arguments"];
        } else if (params.contains("arguments")) {
            return makeInvalidParamsError(id, "arguments must be an object");
        }
    } catch (...) {
        return makeInvalidParamsError(id, "unreadable params");
    }
    const ToolDefinition* tool = registry.find(name);
    if (tool == nullptr) {
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["error"] = makeUnknownToolError(id, name)["error"];
        return response;
    }
    std::string schemaError;
    if (!argumentsMatchSchema(args, tool->inputSchema, &schemaError)) {
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["error"] = makeInvalidParamsError(id, schemaError)["error"];
        return response;
    }
    try {
        nlohmann::json structured = tool->handler(args);
        // Handlers return {structured, summary} packed; unpack here.
        nlohmann::json payload = structured.value("structuredContent", structured);
        std::string summary = structured.value("summary", "ok");
        if (structured.contains("structuredContent")) {
            payload = structured["structuredContent"];
        }
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = wrapToolSuccess(payload, summary);
        return response;
    } catch (const std::exception& ex) {
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = wrapToolError(toolErrorText(ex));
        return response;
    } catch (...) {
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = wrapToolError("tool failed");
        return response;
    }
}
