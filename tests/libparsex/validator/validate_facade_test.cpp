// PAR-112: Validator facade + StrictMode aggregation.
#include <gtest/gtest.h>

#include <parsex/validator/validator.hpp>

TEST(StrictModeTest, WarningsOnlyPassLenientFailStrict) {
    ValidationResult warningsOnly;
    warningsOnly.errors.push_back({.severity = Severity::Warning,
                                   .code = "ref.dest_unrecognized",
                                   .message = "warn"});
    EXPECT_TRUE(Validator::overallPassed(warningsOnly, StrictMode::Lenient));
    EXPECT_FALSE(Validator::overallPassed(warningsOnly, StrictMode::Strict));
    // Strict mode flips the verdict, never the labeled severity.
    EXPECT_EQ(warningsOnly.errors.front().severity, Severity::Warning);
}

TEST(StrictModeTest, ErrorsFailBothModes) {
    ValidationResult withError;
    withError.errors.push_back(
        {.severity = Severity::Error, .code = "can.dlc_mismatch", .message = "bad"});
    EXPECT_FALSE(Validator::overallPassed(withError, StrictMode::Lenient));
    EXPECT_FALSE(Validator::overallPassed(withError, StrictMode::Strict));
}

TEST(StrictModeTest, EmptyPassesBothModes) {
    const ValidationResult empty;
    EXPECT_TRUE(Validator::overallPassed(empty, StrictMode::Lenient));
    EXPECT_TRUE(Validator::overallPassed(empty, StrictMode::Strict));
}

TEST(StrictModeTest, MergePreservesSeverities) {
    ValidationResult errors;
    errors.errors.push_back(
        {.severity = Severity::Error, .code = "a", .message = "a"});
    ValidationResult warnings;
    warnings.errors.push_back(
        {.severity = Severity::Warning, .code = "b", .message = "b"});
    errors.merge(warnings);
    ASSERT_EQ(errors.errors.size(), 2U);
    EXPECT_EQ(errors.errors[0].severity, Severity::Error);
    EXPECT_EQ(errors.errors[1].severity, Severity::Warning);
}
