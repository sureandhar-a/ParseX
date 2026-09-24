#pragma once

#include <cassert>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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

  private:
    std::map<std::string, ToolDefinition> tools_;
    std::vector<std::string> order_;
};
