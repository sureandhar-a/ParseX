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
