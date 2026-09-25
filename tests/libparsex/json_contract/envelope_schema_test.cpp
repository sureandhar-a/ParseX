// Unit tests for the JSON envelope schema (PAR-167).
//
// NOTE (temporary approach): the full `json-schema-validator` integration is
// PAR-164's job. Until then this file uses a small hand-rolled structural
// check that mirrors schemas/envelope.schema.json (required keys, closed
// top-level set, semver pattern, kind enum). Once PAR-164 lands, simplify /
// consolidate this to load the real schema file through json-schema-validator
// instead of duplicating its rules here — do not mistake this helper for the
// project's permanent JSON Schema validation strategy.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

namespace {

namespace fs = std::filesystem;

// Mirrors schemas/envelope.schema.json. Keep in sync until PAR-164 replaces
// this with json-schema-validator.
bool validatesAsEnvelope(const nlohmann::json& doc, std::string* errorOut = nullptr) {
    auto fail = [&](const std::string& msg) {
        if (errorOut != nullptr) {
            *errorOut = msg;
        }
        return false;
    };
    if (!doc.is_object()) {
        return fail("envelope must be a JSON object");
    }
    static const std::set<std::string> kAllowed = {
        "$schema", "contractVersion", "toolVersion", "kind", "payload"};
    static const std::set<std::string> kRequired = {
        "contractVersion", "toolVersion", "kind", "payload"};

    for (const auto& [key, _] : doc.items()) {
        if (kAllowed.find(key) == kAllowed.end()) {
            return fail("unexpected top-level key: " + key);
        }
    }
    for (const auto& key : kRequired) {
        if (!doc.contains(key)) {
            return fail("missing required key: " + key);
        }
    }
    if (!doc.at("contractVersion").is_string()) {
        return fail("contractVersion must be a string");
    }
    static const std::regex kSemver(R"(^\d+\.\d+\.\d+$)");
    if (!std::regex_match(doc.at("contractVersion").get<std::string>(), kSemver)) {
        return fail("contractVersion must match semver ^\\d+\\.\\d+\\.\\d+$");
    }
    if (!doc.at("toolVersion").is_string() || doc.at("toolVersion").get<std::string>().empty()) {
        return fail("toolVersion must be a non-empty string");
    }
    if (!doc.at("kind").is_string()) {
        return fail("kind must be a string");
    }
    const std::string kind = doc.at("kind").get<std::string>();
    if (kind != "validationResult" && kind != "diffReport") {
        return fail("kind must be one of [validationResult, diffReport]");
    }
    if (doc.contains("$schema") && !doc.at("$schema").is_string()) {
        return fail("$schema must be a string when present");
    }
    // payload is intentionally unconstrained: presence alone is enough.
    return true;
}

nlohmann::json validExampleEnvelope() {
    return nlohmann::json{
        {"$schema", "https://parsex.dev/schemas/v1/envelope.json"},
        {"contractVersion", "0.1.0"},
        {"toolVersion", "0.1.0"},
        {"kind", "validationResult"},
        {"payload", nlohmann::json{{"passed", true}}},
    };
}

std::string schemaFilePath() {
#ifdef PARSEX_ENVELOPE_SCHEMA_FILE
    return PARSEX_ENVELOPE_SCHEMA_FILE;
#else
    return "schemas/envelope.schema.json";
#endif
}

}  // namespace

TEST(EnvelopeSchemaTest, SchemaFileParsesAsValidJsonWithExpectedShape) {
    const std::string path = schemaFilePath();
    ASSERT_TRUE(fs::exists(path)) << "missing schema file: " << path;
    std::ifstream in(path);
    ASSERT_TRUE(in.good()) << "cannot open schema file: " << path;
    std::ostringstream raw;
    raw << in.rdbuf();

    nlohmann::json schema = nullptr;
    ASSERT_NO_THROW(schema = nlohmann::json::parse(raw.str()))
        << "envelope.schema.json must parse as valid JSON";
    ASSERT_TRUE(schema.is_object());

    // Minimal meta-schema-shaped check (full meta-schema validation via
    // json-schema-validator arrives in PAR-164).
    EXPECT_EQ(schema.at("$schema").get<std::string>(),
              "https://json-schema.org/draft/2020-12/schema");
    EXPECT_EQ(schema.at("$id").get<std::string>(), "https://parsex.dev/schemas/v1/envelope.json");
    EXPECT_EQ(schema.at("title").get<std::string>(), "ParseX JSON Output Envelope");
    EXPECT_EQ(schema.at("type").get<std::string>(), "object");
    ASSERT_TRUE(schema.contains("properties"));
    ASSERT_TRUE(schema.contains("required"));
    EXPECT_FALSE(schema.at("additionalProperties").get<bool>())
        << "envelope level must stay closed (additionalProperties: false)";

    const auto required = schema.at("required");
    EXPECT_TRUE(std::find(required.begin(), required.end(), "contractVersion") != required.end());
    EXPECT_TRUE(std::find(required.begin(), required.end(), "toolVersion") != required.end());
    EXPECT_TRUE(std::find(required.begin(), required.end(), "kind") != required.end());
    EXPECT_TRUE(std::find(required.begin(), required.end(), "payload") != required.end());

    const auto& props = schema.at("properties");
    EXPECT_EQ(props.at("contractVersion").at("pattern").get<std::string>(), "^\\d+\\.\\d+\\.\\d+$");
    const auto& kindEnum = props.at("kind").at("enum");
    EXPECT_TRUE(std::find(kindEnum.begin(), kindEnum.end(), "validationResult") != kindEnum.end());
    EXPECT_TRUE(std::find(kindEnum.begin(), kindEnum.end(), "diffReport") != kindEnum.end());
}

TEST(EnvelopeSchemaTest, ValidExampleEnvelopePasses) {
    const nlohmann::json doc = validExampleEnvelope();
    std::string error;
    EXPECT_TRUE(validatesAsEnvelope(doc, &error)) << error;
}

TEST(EnvelopeSchemaTest, MissingRequiredFieldFails) {
    nlohmann::json doc = validExampleEnvelope();
    doc.erase("contractVersion");
    std::string error;
    EXPECT_FALSE(validatesAsEnvelope(doc, &error));
    EXPECT_NE(error.find("contractVersion"), std::string::npos) << error;
}

TEST(EnvelopeSchemaTest, ExtraTopLevelKeyFailsGivenClosedSchema) {
    nlohmann::json doc = validExampleEnvelope();
    doc["unexpectedKey"] = "oops";
    std::string error;
    EXPECT_FALSE(validatesAsEnvelope(doc, &error));
    EXPECT_NE(error.find("unexpected"), std::string::npos) << error;
}
