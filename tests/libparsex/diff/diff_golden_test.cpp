// Determinism and golden-file tests for text/JSON output (PAR-134).
//
// To intentionally regenerate the golden files after a deliberate format
// change: PARSEX_REGENERATE_GOLDEN=1 ./build/tests/libparsex/diff/diff_tests
// --gtest_filter='GoldenFileTest.*' && git diff tests/libparsex/diff/golden/
// (then commit the updated goldens after manual review).
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <parsex/diff/diff_match.hpp>
#include <parsex/diff/diff_moves.hpp>
#include <parsex/diff/diff_populate.hpp>
#include <parsex/diff/diff_report.hpp>
#include <parsex/model/parsed_project.hpp>

namespace {

Frame frameWith(const std::string& name, std::uint32_t length, const std::string& tx) {
    Frame f;
    f.common.shortName = name;
    f.length = length;
    if (!tx.empty()) {
        f.transmitters = {tx};
    }
    return f;
}

Signal signalWith(const std::string& name, std::uint32_t startBit) {
    Signal s;
    s.common.shortName = name;
    s.startBit = startBit;
    s.bitLength = 8;
    return s;
}

// Small varied fixture pair: Added, Removed, Moved, Modified + ambiguous
// diagnostic. Keys are unique per element except the intended move pair and
// the ambiguous group.
std::pair<ParsedProject, ParsedProject> goldenFixtures() {
    ParsedProject oldProject;
    ParsedFile oldFile;
    oldFile.frames.push_back(frameWith("FStay", 8, "ECU_A"));
    oldFile.frames.push_back(frameWith("FOld", 8, "ECU_B"));
    oldFile.frames.push_back(frameWith("FMoveOld", 8, "ECU_MOVE"));
    oldFile.frames.push_back(frameWith("AMB1", 16, ""));
    oldFile.frames.push_back(frameWith("AMB2", 16, ""));
    oldFile.signals.push_back(signalWith("SMod", 3));
    oldFile.signals.push_back(signalWith("SStay", 1));
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    newFile.frames.push_back(frameWith("FStay", 8, "ECU_A"));
    newFile.frames.push_back(frameWith("FNew", 4, "ECU_C"));
    newFile.frames.push_back(frameWith("FMoveNew", 8, "ECU_MOVE"));
    newFile.frames.push_back(frameWith("AMB3", 16, ""));
    newFile.frames.push_back(frameWith("AMB4", 16, ""));
    newFile.signals.push_back(signalWith("SMod", 7));
    newFile.signals.push_back(signalWith("SStay", 1));
    newProject.files.push_back(newFile);
    return {oldProject, newProject};
}

DiffReport runFullPipeline(const ParsedProject& oldProject, const ParsedProject& newProject) {
    DiffReport matched = matchParsedProjects(oldProject, newProject);
    DiffReport moved = applyMoveDetection(oldProject, newProject, std::move(matched));
    DiffReport populated = populateFieldDiffs(oldProject, newProject, std::move(moved));
    populated.sortDeterministically();
    return populated;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

}  // namespace

TEST(DeterminismTest, TextAndJsonByteIdenticalAcrossRuns) {
    const auto [oldProject, newProject] = goldenFixtures();
    const DiffReport first = runFullPipeline(oldProject, newProject);
    const DiffReport second = runFullPipeline(oldProject, newProject);

    EXPECT_EQ(first.toText(), second.toText());
    EXPECT_EQ(first.toJson().dump(2), second.toJson().dump(2));
    // Sanity: the fixture actually exercises every category.
    bool sawAdded = false;
    bool sawRemoved = false;
    bool sawMoved = false;
    bool sawModified = false;
    for (const auto& e : first.entries) {
        sawAdded |= e.kind == DiffKind::Added;
        sawRemoved |= e.kind == DiffKind::Removed;
        sawMoved |= e.kind == DiffKind::Moved;
        sawModified |= e.kind == DiffKind::Modified;
    }
    EXPECT_TRUE(sawAdded && sawRemoved && sawMoved && sawModified);
    EXPECT_FALSE(first.diagnostics.empty());
}

TEST(GoldenFileTest, OutputMatchesCommittedGoldens) {
    const auto [oldProject, newProject] = goldenFixtures();
    const DiffReport report = runFullPipeline(oldProject, newProject);

    const std::filesystem::path dir =
        std::filesystem::path(DIFF_GOLDEN_DIR);
    const std::string text = report.toText();
    const std::string json = report.toJson().dump(2) + "\n";

    if (const char* regen = std::getenv("PARSEX_REGENERATE_GOLDEN");
        regen != nullptr && std::string(regen) == "1") {
        writeFile(dir / "example.txt", text);
        writeFile(dir / "example.json", json);
        GTEST_SUCCEED() << "Regenerated golden files in " << dir;
        return;
    }

    EXPECT_EQ(text, readFile(dir / "example.txt"));
    // Structural JSON comparison so key ordering never false-fails.
    EXPECT_EQ(nlohmann::json::parse(json), nlohmann::json::parse(readFile(dir / "example.json")));
}
