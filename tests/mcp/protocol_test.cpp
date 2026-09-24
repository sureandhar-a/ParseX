#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "protocol.hpp"

TEST(EnvelopeChecks, InvalidJsonHasNoEnvelope) {
    // A line that is not JSON at all cannot be validated; recovery finds no id.
    EXPECT_FALSE(tryRecoverId("not json at all").has_value());
    EXPECT_FALSE(tryRecoverId("{{{").has_value());
}

TEST(EnvelopeChecks, MissingVersionFails) {
    nlohmann::json message = {{"id", 1}, {"method", "tools/list"}};
    EXPECT_FALSE(isValidEnvelope(message));
    nlohmann::json response = makeInvalidRequest(idOrNull(message));
    EXPECT_EQ(response["error"]["code"], -32600);
    EXPECT_EQ(response["jsonrpc"], "2.0");
}

TEST(EnvelopeChecks, WrongVersionFails) {
    nlohmann::json message = {{"jsonrpc", "1.0"}, {"id", 2}, {"method", "tools/list"}};
    EXPECT_FALSE(isValidEnvelope(message));
}

TEST(EnvelopeChecks, WellFormedPassesThrough) {
    nlohmann::json message = {{"jsonrpc", "2.0"}, {"id", 3}, {"method", "tools/list"}};
    EXPECT_TRUE(isValidEnvelope(message));
}

TEST(EnvelopeChecks, NotificationWithoutIdPasses) {
    nlohmann::json message = {{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}};
    EXPECT_TRUE(isValidEnvelope(message));
}

TEST(EnvelopeChecks, ParseErrorShape) {
    nlohmann::json response = makeParseError(7);
    EXPECT_EQ(response["error"]["code"], -32700);
    EXPECT_EQ(response["id"], 7);
}
