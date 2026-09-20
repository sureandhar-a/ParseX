// parseProject() with ExplicitList: each named file parses independently via
// parseFile() and lands in ParsedProject.files, in order. No cross-file
// reference resolution yet (later subtask) — resolvedRefs stays empty here.

#include <gtest/gtest.h>

#include <parsex/model/parsed_project.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/parser/release_error.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

std::string readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void writeBytes(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary);
    output << bytes;
}

std::vector<std::string> parsedBasenames(const ParsedProject& project) {
    std::vector<std::string> names;
    names.reserve(project.files.size());
    for (const auto& file : project.files) {
        names.push_back(file.sourcePath.filename().string());
    }
    std::ranges::sort(names);
    return names;
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
    EXPECT_THROW(Parser{}.parseProject(entries, FileDiscoveryMode::LazyOnReference),
                 std::runtime_error);
}

TEST(ParseProjectTest, DirectoryScanFindsSiblingsOnly) {
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "parsex_dirscan_test";
    std::error_code ignored;
    std::filesystem::remove_all(scratch, ignored);
    std::filesystem::create_directories(scratch / "sub");

    // One entry point, two siblings (one uppercase extension), one unrelated
    // file, and one nested file the single-level scan must not reach.
    const std::string body = readBytes(fixture("parsefile_complete.arxml"));
    writeBytes(scratch / "entry.arxml", body);
    writeBytes(scratch / "sibling.arxml", body);
    writeBytes(scratch / "upper.ARXML", body);
    writeBytes(scratch / "notes.txt", "not xml\n");
    writeBytes(scratch / "sub" / "nested.arxml", body);

    const ParsedProject project = Parser{}.parseProject(
        {scratch / "entry.arxml"}, FileDiscoveryMode::DirectoryScan);
    EXPECT_EQ(parsedBasenames(project),
              std::vector<std::string>({"entry.arxml", "sibling.arxml", "upper.ARXML"}));
    for (const auto& file : project.files) {
        EXPECT_EQ(file.autosarRelease, "4.4.0");
    }

    // A second entry point in the same directory adds no duplicates.
    const ParsedProject deduped = Parser{}.parseProject(
        {scratch / "entry.arxml", scratch / "sibling.arxml"},
        FileDiscoveryMode::DirectoryScan);
    EXPECT_EQ(deduped.files.size(), 3U);

    std::filesystem::remove_all(scratch, ignored);
}
