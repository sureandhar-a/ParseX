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
