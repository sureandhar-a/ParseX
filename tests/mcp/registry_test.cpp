#include <gtest/gtest.h>

#include "tool_registry.hpp"

TEST(Registry, HoldsFourToolsInStableOrder) {
    ToolRegistry registry = ToolRegistry::withPlaceholders();
    EXPECT_EQ(registry.size(), 4);
    auto ordered = registry.ordered();
    ASSERT_EQ(ordered.size(), 4);
    EXPECT_EQ(ordered[0]->name, "parse_arxml");
    EXPECT_EQ(ordered[1]->name, "validate_arxml");
    EXPECT_EQ(ordered[2]->name, "diff_arxml");
    EXPECT_EQ(ordered[3]->name, "write_arxml");
    // Second pass returns the same order.
    auto again = registry.ordered();
    ASSERT_EQ(again.size(), 4);
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        EXPECT_EQ(again[i]->name, ordered[i]->name);
    }
}

TEST(Registry, NamesFollowCharacterRule) {
    EXPECT_TRUE(isValidToolName("parse_arxml"));
    EXPECT_TRUE(isValidToolName("a"));
    EXPECT_FALSE(isValidToolName(""));
    EXPECT_FALSE(isValidToolName("has space"));
    EXPECT_FALSE(isValidToolName("bad/name"));
    EXPECT_FALSE(isValidToolName(std::string(129, 'a')));
}
