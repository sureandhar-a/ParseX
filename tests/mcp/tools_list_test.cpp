#include <gtest/gtest.h>

#include "server.hpp"
#include "tool_registry.hpp"

TEST(ToolListing, ReturnsAllFourWithAnnotationsInOrder) {
    ToolRegistry registry = ToolRegistry::withSchemas();
    nlohmann::json result = buildToolsListResult(registry);
    ASSERT_TRUE(result["tools"].is_array());
    ASSERT_EQ(result["tools"].size(), 4);
    EXPECT_EQ(result["tools"][0]["name"], "parse_arxml");
    EXPECT_EQ(result["tools"][1]["name"], "validate_arxml");
    EXPECT_EQ(result["tools"][2]["name"], "diff_arxml");
    EXPECT_EQ(result["tools"][3]["name"], "write_arxml");
    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(result["tools"][i]["annotations"]["readOnlyHint"], true);
        EXPECT_EQ(result["tools"][i]["annotations"]["destructiveHint"], false);
        EXPECT_EQ(result["tools"][i]["annotations"]["idempotentHint"], true);
        EXPECT_EQ(result["tools"][i]["annotations"]["openWorldHint"], false);
    }
    EXPECT_EQ(result["tools"][3]["annotations"]["readOnlyHint"], false);
    EXPECT_EQ(result["tools"][3]["annotations"]["destructiveHint"], true);
    EXPECT_EQ(result["tools"][3]["annotations"]["idempotentHint"], false);
    EXPECT_EQ(result["tools"][3]["annotations"]["openWorldHint"], false);
}

TEST(ToolListing, AcceptsCursorWithoutError) {
    ToolRegistry registry = ToolRegistry::withSchemas();
    nlohmann::json withCursor = buildToolsListResult(registry, "some-cursor");
    nlohmann::json withoutCursor = buildToolsListResult(registry);
    EXPECT_EQ(withCursor, withoutCursor);
}

TEST(WriteAnnotations, GuardedFromSilentRegression) {
    ToolRegistry registry = ToolRegistry::withSchemas();
    const ToolDefinition* write = registry.find("write_arxml");
    ASSERT_NE(write, nullptr);
    EXPECT_EQ(write->annotations["readOnlyHint"], false);
    EXPECT_EQ(write->annotations["destructiveHint"], true);
    EXPECT_EQ(write->annotations["idempotentHint"], false);
    EXPECT_EQ(write->annotations["openWorldHint"], false);
    nlohmann::json listed = buildToolsListResult(registry);
    ASSERT_EQ(listed["tools"].size(), 4);
    EXPECT_EQ(listed["tools"][3]["annotations"], write->annotations);
}

TEST(WriteDescription, StatesPreviewDefaultAndConfirmation) {
    ToolRegistry registry = ToolRegistry::withSchemas();
    const ToolDefinition* write = registry.find("write_arxml");
    ASSERT_NE(write, nullptr);
    const std::string text = write->description;
    EXPECT_NE(text.find("Preview"), std::string::npos);
    EXPECT_NE(text.find("apply:true"), std::string::npos);
    EXPECT_NE(text.find("confirmation"), std::string::npos);
    EXPECT_LT(text.size(), 400);
}
