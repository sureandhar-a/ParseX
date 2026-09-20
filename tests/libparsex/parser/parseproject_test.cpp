// parseProject() with ExplicitList: each named file parses independently via
// parseFile() and lands in ParsedProject.files, in order. No cross-file
// reference resolution yet (later subtask) — resolvedRefs stays empty here.

#include <gtest/gtest.h>

#include <parsex/model/parsed_project.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/parser/release_error.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

}  // namespace

TEST(ParseProjectTest, ExplicitListCollectsThreeFilesInOrder) {
    const std::vector<std::filesystem::path> entries = {
        fixture("parsefile_complete.arxml"),
        fixture("system-4.2.arxml"),
        fixture("release_multiline.arxml"),
    };
    const ParsedProject project =
        Parser{}.parseProject(entries, FileDiscoveryMode::ExplicitList);

    ASSERT_EQ(project.files.size(), 3U);
    EXPECT_EQ(project.files.at(0).autosarRelease, "4.4.0");
    EXPECT_EQ(project.files.at(0).sourcePath, entries.at(0));
    EXPECT_EQ(project.files.at(0).frames.size(), 1U);
    EXPECT_EQ(project.files.at(1).autosarRelease, "4.4.0");
    EXPECT_EQ(project.files.at(1).sourcePath, entries.at(1));
    EXPECT_EQ(project.files.at(1).frames.size(), 8U);
    EXPECT_EQ(project.files.at(2).autosarRelease, "4.2.2");
    EXPECT_EQ(project.files.at(2).sourcePath, entries.at(2));

    // No cross-file resolution yet.
    EXPECT_TRUE(project.resolvedRefs.empty());
}

TEST(ParseProjectTest, OneFailingFileFailsTheWholeCall) {
    // tiny_valid.arxml declares out-of-range autosar_4_0_0.xsd: its
    // UnsupportedReleaseError must escape instead of yielding partial results.
    const std::vector<std::filesystem::path> entries = {
        fixture("parsefile_complete.arxml"),
        fixture("tiny_valid.arxml"),
    };
    EXPECT_THROW(Parser{}.parseProject(entries, FileDiscoveryMode::ExplicitList),
                 UnsupportedReleaseError);
}

TEST(ParseProjectTest, UnimplementedModeThrows) {
    const std::vector<std::filesystem::path> entries = {
        fixture("parsefile_complete.arxml"),
    };
    EXPECT_THROW(Parser{}.parseProject(entries, FileDiscoveryMode::DirectoryScan),
                 std::runtime_error);
    EXPECT_THROW(Parser{}.parseProject(entries, FileDiscoveryMode::LazyOnReference),
                 std::runtime_error);
}
