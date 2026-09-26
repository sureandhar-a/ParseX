#include <gtest/gtest.h>

#include <parsex/schema/schema_registry.hpp>
#include <parsex/schema/schema_resolution_error.hpp>

#include <optional>

#include "tools.hpp"

namespace {

std::string fixture(const std::string& name) {
#ifdef PARSEX_FIXTURES_DIR
    return std::string(PARSEX_FIXTURES_DIR) + "/" + name;
#else
    return "tests/fixtures/" + name;
#endif
}

// User-supplied AUTOSAR XSDs are gitignored (see resources/schemas/README.md):
// skip validation-pass tests where they are absent (e.g. CI), following
// schema_registry_integration_test.cpp's precedent. Any failure other than
// UnsupportedRelease propagates.
bool schemasMissing() {
    try {
        (void)resolveSchema("4.2.2");
        return false;
    } catch (const SchemaResolutionError& err) {
        if (err.reason() != SchemaResolutionReason::UnsupportedRelease) {
            throw;
        }
        return true;
    }
}

}  // namespace

TEST(ReadTools, ParseValidProducesReport) {
    nlohmann::json args = {{"path", fixture("schema_valid.arxml")}};
    nlohmann::json packed;
    ASSERT_NO_THROW(packed = parseTool(args));
    ASSERT_TRUE(packed.contains("structuredContent"));
    EXPECT_EQ(packed["structuredContent"]["kind"], "parseReport");
    EXPECT_TRUE(packed.contains("summary"));
}

TEST(ReadTools, ParseMalformedThrowsForDispatch) {
    nlohmann::json args = {{"path", fixture("malformed_unclosed.arxml")}};
    EXPECT_ANY_THROW(parseTool(args));
}

TEST(ReadTools, ValidateValidPassesWithoutToolError) {
    if (schemasMissing()) {
        GTEST_SKIP() << "user-supplied 4.2.2 schema not present";
    }
    nlohmann::json args = {{"path", fixture("schema_valid.arxml")}};
    nlohmann::json packed;
    ASSERT_NO_THROW(packed = validateTool(args));
    EXPECT_EQ(packed["structuredContent"]["kind"], "validationResult");
    EXPECT_TRUE(packed["structuredContent"]["payload"]["passed"].get<bool>());
}

TEST(ReadTools, ValidateInvalidFailsInBodyNotAsToolError) {
    nlohmann::json args = {{"path", fixture("schema_invalid.arxml")}};
    nlohmann::json packed;
    ASSERT_NO_THROW(packed = validateTool(args));
    EXPECT_FALSE(packed["structuredContent"]["payload"]["passed"].get<bool>());
}

TEST(ReadTools, ValidateUnparseableBecomesFindingNotThrow) {
    nlohmann::json args = {{"path", fixture("malformed_unclosed.arxml")}};
    nlohmann::json packed;
    ASSERT_NO_THROW(packed = validateTool(args));
    EXPECT_FALSE(packed["structuredContent"]["payload"]["passed"].get<bool>());
}

TEST(ReadTools, ValidateStrictFlagRespected) {
    if (schemasMissing()) {
        GTEST_SKIP() << "user-supplied 4.2.2 schema not present";
    }
    nlohmann::json lenient = {{"path", fixture("schema_valid.arxml")}};
    nlohmann::json strict = {{"path", fixture("schema_valid.arxml")}, {"strict", true}};
    nlohmann::json lenientResult = validateTool(lenient);
    nlohmann::json strictResult = validateTool(strict);
    EXPECT_TRUE(lenientResult["structuredContent"]["payload"]["passed"].get<bool>());
    EXPECT_TRUE(strictResult["structuredContent"]["payload"]["passed"].get<bool>());
}

TEST(ReadTools, DiffIdenticalIsEmpty) {
    const std::string file = fixture("schema_valid.arxml");
    nlohmann::json args = {{"basePath", file}, {"targetPath", file}};
    nlohmann::json packed;
    ASSERT_NO_THROW(packed = diffTool(args));
    EXPECT_EQ(packed["structuredContent"]["kind"], "diffReport");
    EXPECT_TRUE(packed["structuredContent"]["payload"]["entries"].empty());
}

TEST(ReadTools, DiffDifferingHasEntries) {
    nlohmann::json args = {{"basePath", fixture("schema_valid.arxml")},
                           {"targetPath", fixture("system-4.2.arxml")}};
    nlohmann::json packed;
    ASSERT_NO_THROW(packed = diffTool(args));
    EXPECT_FALSE(packed["structuredContent"]["payload"]["entries"].empty());
}
