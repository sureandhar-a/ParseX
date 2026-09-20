#include <gtest/gtest.h>
#include <parsex/schema/cached_schema.hpp>
#include <parsex/schema/schema_resolution_result.hpp>

#include <filesystem>
#include <memory>
#include <string>

// Placeholder values only: the real schemaHandle wiring (libxml2-backed
// handle + custom deleter) arrives in the next subtask.
TEST(CachedSchemaTest, ConstructsWithPlaceholderValues) {
    CachedSchema schema;
    schema.autosarRelease = "R21-11";
    schema.sourceXsdPath = std::filesystem::path("resources/schemas/R21-11.xsd");
    schema.schemaHandle = std::shared_ptr<xmlSchema>{};

    EXPECT_EQ(schema.autosarRelease, "R21-11");
    EXPECT_EQ(schema.sourceXsdPath, std::filesystem::path("resources/schemas/R21-11.xsd"));
    EXPECT_EQ(schema.schemaHandle, nullptr);
}

TEST(SchemaResolutionResultTest, ConstructsWithPlaceholderValues) {
    SchemaResolutionResult result;
    result.schema.autosarRelease = "4.2.2";
    result.schema.sourceXsdPath = std::filesystem::path("resources/schemas/4.2.2.xsd");
    result.schema.schemaHandle = std::shared_ptr<xmlSchema>{};

    EXPECT_EQ(result.schema.autosarRelease, "4.2.2");
    EXPECT_EQ(result.schema.sourceXsdPath, std::filesystem::path("resources/schemas/4.2.2.xsd"));
    EXPECT_EQ(result.schema.schemaHandle, nullptr);
    // Default: freshly resolved, not served from cache.
    EXPECT_FALSE(result.wasCacheHit);

    result.wasCacheHit = true;
    EXPECT_TRUE(result.wasCacheHit);
}
