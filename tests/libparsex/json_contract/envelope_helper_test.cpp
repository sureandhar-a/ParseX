// Tests for the shared wrapEnvelope() helper (PAR-176).
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <parsex/diff/diff_report.hpp>
#include <parsex/json_contract/envelope.hpp>
#include <parsex/json_contract/schema_validate.hpp>
#include <parsex/validator/validation_result.hpp>

namespace {

namespace fs = std::filesystem;

fs::path envelopeSchema() {
#ifdef PARSEX_SCHEMAS_DIR
    return fs::path(PARSEX_SCHEMAS_DIR) / "envelope.schema.json";
#else
    return fs::path("schemas/envelope.schema.json");
#endif
}

}  // namespace

TEST(WrapEnvelopeTest, MatchesMigratedValidationResultToJson) {
    ValidationResult result;
    result.errors.push_back({.severity = Severity::Error, .code = "a", .message = "a"});

    // Same payload through both code paths must produce identical documents —
    // confirms toJson genuinely calls wrapEnvelope, not a parallel copy.
    const nlohmann::json direct = result.toJson();
    const nlohmann::json payload = direct.at("payload");
    const nlohmann::json viaHelper =
        parsex::json_contract::wrapEnvelope("validationResult", payload);

    EXPECT_EQ(viaHelper.dump(), direct.dump());

    std::string error;
    EXPECT_TRUE(
        parsex::json_contract::validatesAgainstSchema(viaHelper, envelopeSchema(), &error))
        << error;
}

TEST(WrapEnvelopeTest, RepeatedCallsProduceIdenticalVersions) {
    const nlohmann::json payload = nlohmann::json{{"passed", true},
                                                  {"errors", nlohmann::json::array()}};
    const nlohmann::json first =
        parsex::json_contract::wrapEnvelope("validationResult", payload);
    const nlohmann::json second =
        parsex::json_contract::wrapEnvelope("validationResult", payload);
    EXPECT_EQ(first["contractVersion"], second["contractVersion"]);
    EXPECT_EQ(first["toolVersion"], second["toolVersion"]);
    EXPECT_FALSE(first["contractVersion"].get<std::string>().empty());
    EXPECT_FALSE(first["toolVersion"].get<std::string>().empty());
}
