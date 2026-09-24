#include <gtest/gtest.h>

#include "tool_registry.hpp"
#include "tool_schemas.hpp"

TEST(Schemas, AllFourHaveCompletePairs) {
    ToolRegistry registry = ToolRegistry::withSchemas();
    EXPECT_EQ(registry.size(), 4);

    const ToolDefinition* parse = registry.find("parse_arxml");
    ASSERT_NE(parse, nullptr);
    EXPECT_EQ(parse->inputSchema["required"][0], "path");
    EXPECT_TRUE(parse->inputSchema["properties"].contains("path"));
    ASSERT_TRUE(parse->outputSchema.has_value());

    const ToolDefinition* validate = registry.find("validate_arxml");
    ASSERT_NE(validate, nullptr);
    EXPECT_TRUE(validate->inputSchema["properties"].contains("path"));
    EXPECT_TRUE(validate->inputSchema["properties"].contains("strict"));
    ASSERT_TRUE(validate->outputSchema.has_value());

    const ToolDefinition* diff = registry.find("diff_arxml");
    ASSERT_NE(diff, nullptr);
    EXPECT_TRUE(diff->inputSchema["properties"].contains("basePath"));
    EXPECT_TRUE(diff->inputSchema["properties"].contains("targetPath"));
    ASSERT_TRUE(diff->outputSchema.has_value());

    const ToolDefinition* write = registry.find("write_arxml");
    ASSERT_NE(write, nullptr);
    EXPECT_TRUE(write->inputSchema["properties"].contains("path"));
    EXPECT_TRUE(write->inputSchema["properties"].contains("outputPath"));
    EXPECT_TRUE(write->inputSchema["properties"].contains("apply"));
    ASSERT_TRUE(write->outputSchema.has_value());
}

TEST(Schemas, InputsAreClosedObjects) {
    EXPECT_EQ(parseInputSchema()["additionalProperties"], false);
    EXPECT_EQ(validateInputSchema()["additionalProperties"], false);
    EXPECT_EQ(diffInputSchema()["additionalProperties"], false);
    EXPECT_EQ(writeInputSchema()["additionalProperties"], false);
    EXPECT_EQ(writeInputSchema()["properties"]["apply"]["default"], false);
    EXPECT_EQ(validateInputSchema()["properties"]["strict"]["default"], false);
}
