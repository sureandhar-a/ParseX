#include <gtest/gtest.h>

#include "server.hpp"

TEST(Versioning, MatchingVersionPassesThrough) {
    nlohmann::json message = {{"jsonrpc", "2.0"},
                              {"id", 1},
                              {"method", "tools/list"},
                              {"_meta", {{"io.modelcontextprotocol/protocolVersion", "2026-07-28"}}}};
    auto version = extractProtocolVersion(message);
    ASSERT_TRUE(version.has_value());
    EXPECT_TRUE(isSupportedVersion(*version));
}

TEST(Versioning, MissingVersionProceeds) {
    nlohmann::json message = {{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}};
    EXPECT_FALSE(extractProtocolVersion(message).has_value());
}
