// Unit tests for JSON naming / tolerance conventions (PAR-169).
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

namespace fs = std::filesystem;

// Precise-enough camelCase rule for ParseX (see schemas/CONVENTIONS.md §1):
//   ^[a-z$][a-zA-Z0-9$]*$  with NO '_' anywhere.
// This is deliberately slightly stricter than the Google guide's
// "first character letter/_/$": we ban '_' entirely so snake_case
// (which always contains '_') can never slip through, while "$schema"
// (leading '$', rest lowercase) still passes. Documented here so the
// exact enforced rule is visible next to the test, not just in the doc.
bool isCamelCaseKey(const std::string& key) {
    static const std::regex kCamel(R"(^[a-z$][a-zA-Z0-9$]*$)");
    if (key.find('_') != std::string::npos) {
        return false;
    }
    return std::regex_match(key, kCamel);
}

void collectNonCamelCaseKeys(const nlohmann::json& node, std::vector<std::string>& out) {
    if (node.is_object()) {
        for (const auto& [key, value] : node.items()) {
            if (!isCamelCaseKey(key)) {
                out.push_back(key);
            }
            collectNonCamelCaseKeys(value, out);
        }
    } else if (node.is_array()) {
        for (const auto& item : node) {
            collectNonCamelCaseKeys(item, out);
        }
    }
}

// Minimal lenient envelope reader (PAR-169 step 4): reads known fields,
// ignores everything else — the concrete code path the tolerance test
// verifies against. Uses nlohmann/json default lenient key access
// (.contains() / .value()), never a strict closed-schema parse.
struct ParsedEnvelope {
    std::string contractVersion;
    std::string toolVersion;
    std::string kind;
    nlohmann::json payload;
};

ParsedEnvelope parseEnvelopeLenient(const nlohmann::json& doc) {
    ParsedEnvelope env;
    env.contractVersion = doc.value("contractVersion", "");
    env.toolVersion = doc.value("toolVersion", "");
    env.kind = doc.value("kind", "");
    env.payload = doc.contains("payload") ? doc.at("payload") : nlohmann::json{};
    return env;
}

nlohmann::json exampleEnvelope() {
    return nlohmann::json{
        {"$schema", "https://parsex.dev/schemas/v1/envelope.json"},
        {"contractVersion", "0.1.0"},
        {"toolVersion", "0.1.0"},
        {"kind", "validationResult"},
        {"payload", nlohmann::json{{"passed", true}}},
    };
}

}  // namespace

TEST(ConventionsTest, ConventionsDocExistsWithAllThreeRules) {
#ifdef PARSEX_CONVENTIONS_FILE
    const std::string path = PARSEX_CONVENTIONS_FILE;
#else
    const std::string path = "schemas/CONVENTIONS.md";
#endif
    ASSERT_TRUE(fs::exists(path)) << "missing conventions doc: " << path;
    std::ifstream in(path);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("camelCase"), std::string::npos);
    EXPECT_NE(text.find("omit"), std::string::npos);
    EXPECT_NE(text.find("ignore unknown"), std::string::npos);
    // Honest sourcing: doc must state the tolerance rule is NOT from Google.
    EXPECT_NE(text.find("NOT"), std::string::npos);
    EXPECT_NE(text.find("Google"), std::string::npos);
}

TEST(ConventionsTest, EnvelopeExampleHasZeroNamingViolations) {
    const nlohmann::json doc = exampleEnvelope();
    std::vector<std::string> bad;
    collectNonCamelCaseKeys(doc, bad);
    EXPECT_TRUE(bad.empty()) << "non-camelCase keys found";
}

TEST(ConventionsTest, SnakeCaseKeysAreFlagged) {
    const nlohmann::json doc = nlohmann::json{
        {"contract_version", "0.1.0"},
        {"toolVersion", "0.1.0"},
    };
    std::vector<std::string> bad;
    collectNonCamelCaseKeys(doc, bad);
    ASSERT_EQ(bad.size(), 1U);
    EXPECT_EQ(bad[0], "contract_version");
}

TEST(ConventionsTest, UnknownTopLevelKeyIsIgnored) {
    nlohmann::json doc = exampleEnvelope();
    doc["futureField"] = "added in a later minor version";
    ParsedEnvelope env;
    EXPECT_NO_THROW(env = parseEnvelopeLenient(doc));
    EXPECT_EQ(env.contractVersion, "0.1.0");
    EXPECT_EQ(env.kind, "validationResult");
    EXPECT_TRUE(env.payload.contains("passed"));
}
