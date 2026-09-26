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

// --- Id recovery: same answers as before, now linear-time --------------------

TEST(IdRecovery, RecoversNumberStringAndNull) {
    EXPECT_EQ(tryRecoverId(R"({"jsonrpc":"2.0","id":42,"method":)").value(), 42);
    EXPECT_EQ(tryRecoverId(R"({"id" :  -7 ,)").value(), -7);
    EXPECT_EQ(tryRecoverId(R"({"id":"abc","method")").value(), "abc");
    EXPECT_TRUE(tryRecoverId(R"({"id":null,)").value().is_null());
}

TEST(IdRecovery, SkipsUnusableOccurrenceAndTakesTheNextOne) {
    // First "id" has no colon/value; the regex this replaced kept searching.
    EXPECT_EQ(tryRecoverId(R"({"id" x, "params":{"id":5})").value(), 5);
}

TEST(IdRecovery, RejectsOverflowingNumber) {
    EXPECT_FALSE(tryRecoverId(R"({"id":99999999999999999999999,)").has_value());
}

TEST(IdRecovery, LongUnterminatedStringDoesNotCrash) {
    // 1 MB after `"id":"` with no closing quote used to overflow the stack
    // inside std::regex_search.
    const std::string line = R"({"id":")" + std::string(1'000'000, 'A');
    EXPECT_FALSE(tryRecoverId(line).has_value());
}

// --- Depth guard ---------------------------------------------------------------

TEST(DepthGuard, ShallowMessagesPass) {
    EXPECT_FALSE(exceedsMaxDepth(R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})"));
    EXPECT_FALSE(exceedsMaxDepth(std::string(kMaxJsonDepth, '[') + std::string(kMaxJsonDepth, ']')));
}

TEST(DepthGuard, OneLevelOverTheCapIsRejected) {
    EXPECT_TRUE(exceedsMaxDepth(std::string(kMaxJsonDepth + 1, '[')));
    EXPECT_TRUE(exceedsMaxDepth(std::string(kMaxJsonDepth + 1, '{')));
}

TEST(DepthGuard, BracketsInsideStringsAreIgnored) {
    const std::string brackets(kMaxJsonDepth * 4, '[');
    EXPECT_FALSE(exceedsMaxDepth(R"({"path":")" + brackets + R"("})"));
    // An escaped quote does not end the string, so the brackets after it stay
    // inside the literal.
    EXPECT_FALSE(exceedsMaxDepth(R"({"path":"a\")" + brackets + R"("})"));
}

TEST(DepthGuard, ClosingBracketsBringDepthBackDown) {
    std::string line;
    for (int i = 0; i < 1000; ++i) {
        line += "[[]]"; // never deeper than 2
    }
    EXPECT_FALSE(exceedsMaxDepth(line));
}

TEST(DepthGuard, HugeNestingIsRejectedWithoutParsing) {
    const std::size_t depth = 1'000'000;
    const std::string line = std::string(depth, '[') + std::string(depth, ']');
    EXPECT_TRUE(exceedsMaxDepth(line));
}

TEST(DepthGuard, ErrorShape) {
    const nlohmann::json response = makeNestingTooDeep(9);
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 9);
    EXPECT_EQ(response["error"]["code"], -32600);
    EXPECT_EQ(response["error"]["data"]["maxDepth"], kMaxJsonDepth);
}
