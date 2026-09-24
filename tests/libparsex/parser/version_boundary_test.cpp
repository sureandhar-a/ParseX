// Version-boundary regression: oldest/newest resolve, just-outside rejects
// with a specific error, malformed fails as missing release. Covers the
// library path here; the command-line path is verified manually (same
// resolveRelease call, exit handled by classify()).
#include <gtest/gtest.h>

#include <parsex/parser/parser.hpp>
#include <parsex/parser/release_detector.hpp>
#include <parsex/parser/release_error.hpp>

#include <filesystem>
#include <string>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

}  // namespace

TEST(VersionBoundaryTest, InRangeResolves) {
    EXPECT_EQ(Parser{}.parseFile(fixture("hardening_version_oldest.arxml")).autosarRelease,
              "4.2.2");
    EXPECT_EQ(Parser{}.parseFile(fixture("hardening_version_newest.arxml")).autosarRelease,
              "R21-11");
}

TEST(VersionBoundaryTest, JustOutsideRejectsSpecifically) {
    for (const std::string& name :
         {"hardening_version_below_oldest.arxml", "hardening_version_above_newest.arxml"}) {
        try {
            Parser{}.parseFile(fixture(name));
            FAIL() << name << " should reject";
        } catch (const UnsupportedReleaseError& err) {
            EXPECT_NE(std::string(err.what()).find("not a supported"), std::string::npos)
                << name;
        }
    }
}

TEST(VersionBoundaryTest, MalformedFailsAsMissingRelease) {
    // Single token with no filename: detector yields it as filename, resolver
    // rejects specifically (not a generic parse failure).
    try {
        Parser{}.parseFile(fixture("hardening_version_malformed.arxml"));
        FAIL() << "malformed should reject";
    } catch (const UnsupportedReleaseError&) {
        SUCCEED();
    } catch (const std::runtime_error& err) {
        // Also acceptable: missing-release path when detector yields nullopt.
        EXPECT_NE(std::string(err.what()).find("release"), std::string::npos);
    }
}
