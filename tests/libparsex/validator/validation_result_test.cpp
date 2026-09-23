// Smoke test for ValidationError/ValidationResult (PAR-98): construction,
// hasErrors()/hasWarnings()/merge() on empty and non-empty results.
#include <gtest/gtest.h>

#include <parsex/json_contract/version.hpp>
#include <parsex/validator/validation_result.hpp>
#include <parsex/version.hpp>

TEST(ValidationResultTest, EmptyHasNeitherErrorsNorWarnings) {
    ValidationResult result;
    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(result.hasErrors());
    EXPECT_FALSE(result.hasWarnings());
}

TEST(ValidationResultTest, ErrorAndWarningFlags) {
    ValidationResult result;
    result.errors.push_back(
        {.severity = Severity::Error, .code = "schema.invalid", .message = "bad"});
    EXPECT_TRUE(result.hasErrors());
    EXPECT_FALSE(result.hasWarnings());

    result.errors.push_back(
        {.severity = Severity::Warning, .code = "ref.dest_unrecognized", .message = "warn"});
    EXPECT_TRUE(result.hasErrors());
    EXPECT_TRUE(result.hasWarnings());
}

TEST(ValidationResultTest, MergeAppends) {
    ValidationResult a;
    a.errors.push_back({.severity = Severity::Error, .code = "a", .message = "a"});
    ValidationResult b;
    b.errors.push_back({.severity = Severity::Warning, .code = "b", .message = "b"});

    a.merge(b);
    EXPECT_EQ(a.errors.size(), 2U);
    EXPECT_TRUE(a.hasErrors());
    EXPECT_TRUE(a.hasWarnings());
    // Merging must not mutate the source.
    EXPECT_EQ(b.errors.size(), 1U);
}

TEST(ValidationResultTest, ToJsonProducesEnvelopeWrappedShape) {
    ValidationResult result;
    result.errors.push_back({.severity = Severity::Error,
                             .code = "can.dlc_mismatch",
                             .message = "DLC 8 != 64",
                             .path = std::string("/Cluster/CAN/Frame")});

    const nlohmann::json j = result.toJson();
    EXPECT_EQ(j["kind"], "validationResult");
    EXPECT_EQ(j["contractVersion"], std::string(parsex::json_contract::kContractVersion));
    EXPECT_EQ(j["toolVersion"], std::string(libparsexVersion()));
    ASSERT_TRUE(j.contains("payload"));

    const nlohmann::json& payload = j["payload"];
    EXPECT_FALSE(payload["passed"].get<bool>());
    ASSERT_EQ(payload["errors"].size(), 1U);
    EXPECT_EQ(payload["errors"][0]["severity"], "error");
    EXPECT_EQ(payload["errors"][0]["code"], "can.dlc_mismatch");
    EXPECT_EQ(payload["errors"][0]["path"], "/Cluster/CAN/Frame");
    // Omit-optional: unset location/expectedType/actualType are missing, not null.
    EXPECT_FALSE(payload["errors"][0].contains("location"));
    EXPECT_FALSE(payload["errors"][0].contains("expectedType"));
}

TEST(ValidationResultTest, RoundTripPreservesErrors) {
    ValidationResult original;
    original.errors.push_back({.severity = Severity::Error, .code = "a", .message = "a"});
    original.errors.push_back({.severity = Severity::Warning,
                               .code = "b",
                               .message = "b",
                               .expectedType = std::string("FRAME"),
                               .actualType = std::string("PDU")});

    const ValidationResult restored = ValidationResult::fromJson(original.toJson());
    ASSERT_EQ(restored.errors.size(), 2U);
    EXPECT_EQ(restored.errors[0].severity, Severity::Error);
    EXPECT_EQ(restored.errors[0].code, "a");
    EXPECT_FALSE(restored.errors[0].expectedType.has_value());
    EXPECT_EQ(restored.errors[1].severity, Severity::Warning);
    EXPECT_EQ(restored.errors[1].expectedType, "FRAME");
    EXPECT_EQ(restored.errors[1].actualType, "PDU");
}
