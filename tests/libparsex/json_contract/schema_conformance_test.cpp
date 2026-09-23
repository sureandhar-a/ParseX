// Conformance-helper tests with teeth (PAR-175): the shared
// validatesAgainstSchema() helper must pass correct envelopes and reject
// deliberately-broken documents (missing required key, snake_case field).
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <parsex/json_contract/schema_validate.hpp>

namespace {

namespace fs = std::filesystem;

fs::path envelopeSchema() {
#ifdef PARSEX_SCHEMAS_DIR
    return fs::path(PARSEX_SCHEMAS_DIR) / "envelope.schema.json";
#else
    return fs::path("schemas/envelope.schema.json");
#endif
}

nlohmann::json validValidationEnvelope() {
    return nlohmann::json{
        {"$schema", "https://parsex.dev/schemas/v1/envelope.json"},
        {"contractVersion", "1.0.0"},
        {"toolVersion", "1.0.0"},
        {"kind", "validationResult"},
        {"payload", nlohmann::json{{"passed", true}, {"errors", nlohmann::json::array()}}},
    };
}

}  // namespace

TEST(SchemaConformanceTest, ValidEnvelopePasses) {
    std::string error;
    EXPECT_TRUE(
        parsex::json_contract::validatesAgainstSchema(validValidationEnvelope(), envelopeSchema(),
                                                      &error))
        << error;
}

TEST(SchemaConformanceTest, MissingContractVersionFails) {
    nlohmann::json doc = validValidationEnvelope();
    doc.erase("contractVersion");
    std::string error;
    EXPECT_FALSE(parsex::json_contract::validatesAgainstSchema(doc, envelopeSchema(), &error));
    EXPECT_FALSE(error.empty()) << "failure must carry validator diagnostics";
}

TEST(SchemaConformanceTest, SnakeCaseFieldFails) {
    nlohmann::json doc = validValidationEnvelope();
    doc["payload"]["errors"].push_back(nlohmann::json{
        {"severity", "error"}, {"code", "x"}, {"message", "y"}, {"expected_type", "FRAME"}});
    std::string error;
    EXPECT_FALSE(parsex::json_contract::validatesAgainstSchema(doc, envelopeSchema(), &error));
    EXPECT_FALSE(error.empty()) << "failure must carry validator diagnostics";
}
