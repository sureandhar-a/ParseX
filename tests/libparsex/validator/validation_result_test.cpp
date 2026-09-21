// Smoke test for ValidationError/ValidationResult (PAR-98): construction,
// hasErrors()/hasWarnings()/merge() on empty and non-empty results.
#include <gtest/gtest.h>

#include <parsex/validator/validation_result.hpp>

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
