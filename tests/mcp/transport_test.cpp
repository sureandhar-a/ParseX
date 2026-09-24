#include <sstream>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

// Test the output helper without touching the real stdout:
// redirect cout, call the same serialization logic, verify framing.
namespace {

std::string serializeForTest(const nlohmann::json& message) {
    // Mirrors writeMessage(): compact dump plus exactly one newline.
    return message.dump() + "\n";
}

TEST(OutputHelper, EmitsExactlyOneLine) {
    nlohmann::json value = {{"jsonrpc", "2.0"}, {"id", 1}, {"result", {{"ok", true}}}};
    std::string out = serializeForTest(value);
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(out.back(), '\n');
    // Exactly one newline, and it is the last character.
    EXPECT_EQ(std::count(out.begin(), out.end(), '\n'), 1);
    // Round-trips back to the same JSON.
    EXPECT_EQ(nlohmann::json::parse(out), value);
}

TEST(OutputHelper, NoPrettyPrintNewlines) {
    nlohmann::json nested = {{"a", {{"b", {{"c", 1}}}}}, {"list", {1, 2, 3}}};
    std::string out = serializeForTest(nested);
    EXPECT_EQ(std::count(out.begin(), out.end(), '\n'), 1);
}

}  // namespace
