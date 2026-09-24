#include <gtest/gtest.h>

#include "tools.hpp"

namespace {

std::string fixture(const std::string& name) {
#ifdef PARSEX_FIXTURES_DIR
    return std::string(PARSEX_FIXTURES_DIR) + "/" + name;
#else
    return "tests/fixtures/" + name;
#endif
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
    nlohmann::json lenient = {{"path", fixture("schema_valid.arxml")}};
    nlohmann::json strict = {{"path", fixture("schema_valid.arxml")}, {"strict", true}};
    nlohmann::json lenientResult = validateTool(lenient);
    nlohmann::json strictResult = validateTool(strict);
    EXPECT_TRUE(lenientResult["structuredContent"]["payload"]["passed"].get<bool>());
    EXPECT_TRUE(strictResult["structuredContent"]["payload"]["passed"].get<bool>());
}
