#include <gtest/gtest.h>

#include "dispatch.hpp"
#include "tool_registry.hpp"

TEST(Dispatch, UnknownToolIsProtocolError) {
    ToolRegistry registry = ToolRegistry::withSchemas();
    nlohmann::json response =
        dispatchToolsCall(1, {{"name", "nope"}, {"arguments", nlohmann::json::object()}}, registry);
    ASSERT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32602);
}

TEST(Dispatch, SchemaViolationIsProtocolError) {
    ToolRegistry registry = ToolRegistry::withSchemas();
    // parse_arxml requires path; empty args must fail schema.
    nlohmann::json response = dispatchToolsCall(
        2, {{"name", "parse_arxml"}, {"arguments", nlohmann::json::object()}}, registry);
    ASSERT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32602);
}

TEST(Dispatch, SuccessWrapsResult) {
    ToolRegistry registry;
    ToolDefinition definition;
    definition.name = "echo_tool";
    definition.title = "echo_tool";
    definition.inputSchema = {{"type", "object"}};
    definition.handler = [](const nlohmann::json& args) {
        nlohmann::json packed;
        packed["structuredContent"] = {{"echo", true}};
        packed["summary"] = "echo ok";
        return packed;
    };
    registry.registerTool(std::move(definition));
    nlohmann::json response = dispatchToolsCall(
        3, {{"name", "echo_tool"}, {"arguments", nlohmann::json::object()}}, registry);
    ASSERT_TRUE(response.contains("result"));
    EXPECT_EQ(response["result"]["resultType"], "complete");
    EXPECT_TRUE(response["result"].contains("structuredContent"));
}

TEST(Dispatch, EngineFailureBecomesToolError) {
    ToolRegistry registry;
    ToolDefinition definition;
    definition.name = "boom";
    definition.title = "boom";
    definition.inputSchema = {{"type", "object"}};
    definition.handler = [](const nlohmann::json&) -> nlohmann::json {
        throw std::runtime_error("engine exploded");
    };
    registry.registerTool(std::move(definition));
    nlohmann::json response = dispatchToolsCall(
        4, {{"name", "boom"}, {"arguments", nlohmann::json::object()}}, registry);
    ASSERT_TRUE(response.contains("result"));
    EXPECT_EQ(response["result"]["isError"], true);
}
