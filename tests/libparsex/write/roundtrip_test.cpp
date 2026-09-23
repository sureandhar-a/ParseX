// Round-trip helper tests (PAR-154): parse -> write -> reparse -> compare
// via DiffEngine, with toText() on failure; plus a sensitivity check proving
// the helper actually detects real discrepancies.

#include <gtest/gtest.h>

#include <parsex/diff/diff_engine.hpp>
#include <parsex/model/parsed_project.hpp>
#include <parsex/parser/parser.hpp>

#include "roundtrip_helper.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    return path;
}

// Trivial single-Frame fixture: schemaLocation resolves to 4.4.0, one FRAME
// with LENGTH and no refs (no cross-file resolution needed beyond itself).
constexpr const char* kSingleFrame =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<AUTOSAR xmlns=\"http://autosar.org/schema/r4.0\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xsi:schemaLocation=\"http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd\">\n"
    "  <AR-PACKAGES>\n"
    "    <AR-PACKAGE>\n"
    "      <SHORT-NAME>Sys</SHORT-NAME>\n"
    "      <ELEMENTS>\n"
    "        <FRAME>\n"
    "          <SHORT-NAME>OnlyFrame</SHORT-NAME>\n"
    "          <LENGTH>8</LENGTH>\n"
    "        </FRAME>\n"
    "      </ELEMENTS>\n"
    "    </AR-PACKAGE>\n"
    "  </AR-PACKAGES>\n"
    "</AUTOSAR>\n";

}  // namespace

TEST(RoundTripHelperTest, TrivialSingleFrameFixtureReturnsEmptyDiff) {
    const auto path = writeTemp("parsex_roundtrip_trivial.arxml", kSingleFrame);
    EXPECT_TRUE(roundTripDiff(path).empty()) << roundTripDiff(path).toText();
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(RoundTripHelperTest, CompleteFixtureRoundTripsCleanly) {
    assertRoundTripsCleanly(fixture("parsefile_complete.arxml"));
}

TEST(RoundTripHelperTest, BrokenWriteIsDetectedWithFieldDetails) {
    // Sensitivity check: dropping a field must surface as a non-empty report
    // naming it — proving the helper never passes vacuously.
    ParsedProject project = Parser{}.parseProject({fixture("parsefile_complete.arxml")},
                                                  FileDiscoveryMode::ExplicitList);
    ASSERT_EQ(project.files.size(), 1U);
    ASSERT_EQ(project.files.front().signals.size(), 1U);
    ParsedProject broken = project;
    broken.files.front().signals.clear();
    const DiffReport report = DiffEngine{}.diff(project, broken);
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.toText().find("Signal_1"), std::string::npos);
}
