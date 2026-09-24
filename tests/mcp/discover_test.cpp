#include <gtest/gtest.h>

#include "server.hpp"

TEST(Identity, DiscoverShape) {
    nlohmann::json result = buildDiscoverResult();
    EXPECT_EQ(result["resultType"], "complete");
    ASSERT_TRUE(result["supportedVersions"].is_array());
    ASSERT_EQ(result["supportedVersions"].size(), 1);
    EXPECT_EQ(result["supportedVersions"][0], "2026-07-28");
    EXPECT_FALSE(result["capabilities"]["tools"]["listChanged"].get<bool>());
    EXPECT_FALSE(result["capabilities"].contains("resources"));
    EXPECT_FALSE(result["capabilities"].contains("prompts"));
    EXPECT_EQ(result["_meta"]["io.modelcontextprotocol/serverInfo"]["name"], "parsex-mcp");
    EXPECT_EQ(result["_meta"]["io.modelcontextprotocol/serverInfo"]["version"],
              std::string(libparsexVersion()));
}
