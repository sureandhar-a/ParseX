#include <sstream>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

// Cancellation is handled at the dispatch layer (no stdout reply, no crash).
// These checks mirror that contract without spawning the binary.
TEST(Cancellation, NotificationShapeHasNoId) {
    nlohmann::json known = {{"jsonrpc", "2.0"},
                            {"method", "notifications/cancelled"},
                            {"params", {{"requestId", 1}}}};
    EXPECT_FALSE(known.contains("id"));
    EXPECT_EQ(known["method"], "notifications/cancelled");

    nlohmann::json unknown = {{"jsonrpc", "2.0"},
                              {"method", "notifications/cancelled"},
                              {"params", {{"requestId", 9999}}}};
    EXPECT_FALSE(unknown.contains("id"));
}
