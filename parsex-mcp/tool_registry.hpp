#pragma once

#include <cassert>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tool_schemas.hpp"

// Single definition point for every tool, read by both listing and calling,
// so a tool is declared once and used everywhere.

struct ToolDefinition {
    std::string name;
    std::string title;
    std::string description;
    nlohmann::json inputSchema = nlohmann::json::object();
    std::optional<nlohmann::json> outputSchema = std::nullopt;
    nlohmann::json annotations = nlohmann::json::object();
    std::function<nlohmann::json(const nlohmann::json& arguments)> handler;
};

inline bool isValidToolName(const std::string& name) {
    if (name.empty() || name.size() > 128) {
        return false;
    }
    static const std::regex allowed("^[A-Za-z0-9_.-]+$");
    return std::regex_match(name, allowed);
}

class ToolRegistry {
  public:
    void registerTool(ToolDefinition definition) {
        // Own names must always follow the naming rule.
        assert(isValidToolName(definition.name) && "tool name violates naming rule");
        if (tools_.find(definition.name) == tools_.end()) {
            order_.push_back(definition.name);
        }
        tools_[definition.name] = std::move(definition);
    }

    const ToolDefinition* find(const std::string& name) const {
        auto it = tools_.find(name);
        return it == tools_.end() ? nullptr : &it->second;
    }

    void setHandler(const std::string& name,
                    std::function<nlohmann::json(const nlohmann::json&)> handler) {
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            it->second.handler = std::move(handler);
        }
    }

    // Stable registration order for deterministic listing.
    std::vector<const ToolDefinition*> ordered() const {
        std::vector<const ToolDefinition*> out;
        out.reserve(order_.size());
        for (const auto& name : order_) {
            auto it = tools_.find(name);
            if (it != tools_.end()) {
                out.push_back(&it->second);
            }
        }
        return out;
    }

    std::size_t size() const {
        return tools_.size();
    }

    // Four placeholders wired at startup. Schemas arrive with the next piece,
    // handlers with later pieces.
    static ToolRegistry withPlaceholders() {
        ToolRegistry registry;
        for (const char* name : {"parse_arxml", "validate_arxml", "diff_arxml", "write_arxml"}) {
            ToolDefinition definition;
            definition.name = name;
            definition.title = name;
            definition.description = "";
            definition.inputSchema = {{"type", "object"}};
            definition.handler = [](const nlohmann::json&) { return nlohmann::json::object(); };
            registry.registerTool(std::move(definition));
        }
        return registry;
    }

    // Full schemas for all four tools, reusing the report shapes.
    // Annotations are hints, not access control: clients must treat them as
    // untrusted metadata when deciding whether to gate confirmation prompts.
    static ToolRegistry withSchemas() {
        const nlohmann::json readOnlyAnnotations = {{"readOnlyHint", true},
                                                    {"destructiveHint", false},
                                                    {"idempotentHint", true},
                                                    {"openWorldHint", false}};
        const nlohmann::json writeAnnotations = {{"readOnlyHint", false},
                                                 {"destructiveHint", true},
                                                 {"idempotentHint", false},
                                                 {"openWorldHint", false}};
        ToolRegistry registry;
        {
            ToolDefinition definition;
            definition.name = "parse_arxml";
            definition.title = "parse_arxml";
            definition.description = "Parse an ARXML file and report its structure";
            definition.inputSchema = parseInputSchema();
            definition.outputSchema = parseOutputSchema();
            definition.annotations = readOnlyAnnotations;
            definition.handler = [](const nlohmann::json&) { return nlohmann::json::object(); };
            registry.registerTool(std::move(definition));
        }
        {
            ToolDefinition definition;
            definition.name = "validate_arxml";
            definition.title = "validate_arxml";
            definition.description = "Validate an ARXML file";
            definition.inputSchema = validateInputSchema();
            definition.outputSchema = validateOutputSchema();
            definition.annotations = readOnlyAnnotations;
            definition.handler = [](const nlohmann::json&) { return nlohmann::json::object(); };
            registry.registerTool(std::move(definition));
        }
        {
            ToolDefinition definition;
            definition.name = "diff_arxml";
            definition.title = "diff_arxml";
            definition.description = "Diff two ARXML files";
            definition.inputSchema = diffInputSchema();
            definition.outputSchema = diffOutputSchema();
            definition.annotations = readOnlyAnnotations;
            definition.handler = [](const nlohmann::json&) { return nlohmann::json::object(); };
            registry.registerTool(std::move(definition));
        }
        {
            ToolDefinition definition;
            definition.name = "write_arxml";
            definition.title = "write_arxml";
            definition.description = "Preview or apply a safe edit (preview by default)";
            definition.inputSchema = writeInputSchema();
            definition.outputSchema = writeOutputSchema();
            definition.annotations = writeAnnotations;
            definition.handler = [](const nlohmann::json&) { return nlohmann::json::object(); };
            registry.registerTool(std::move(definition));
        }
        return registry;
    }

  private:
    std::map<std::string, ToolDefinition> tools_;
    std::vector<std::string> order_;
};
