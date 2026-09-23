// End-to-end multi-category tests for DiffEngine::diff() (PAR-136).
#include <gtest/gtest.h>

#include <parsex/diff/diff_engine.hpp>
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

Signal signalWith(const std::string& name, std::uint32_t startBit, double factor = 1.0) {
    Signal s;
    s.common.shortName = name;
    s.startBit = startBit;
    s.bitLength = 8;
    s.factor = factor;
    return s;
}

int countKind(const DiffReport& report, DiffKind kind, const std::string& type = "") {
    int n = 0;
    for (const auto& e : report.entries) {
        if (e.kind == kind && (type.empty() || e.elementType == type)) {
            ++n;
        }
    }
    return n;
}

// Combined fixture: Added, Removed, plain Moved, Moved+changed, Modified,
// ambiguous diagnostics, plus unchanged elements that must stay silent.
std::pair<ParsedProject, ParsedProject> combinedFixtures() {
    ParsedProject oldProject;
    ParsedFile oldFile;
    oldFile.frames.push_back(frameWith("FStay", 8, "ECU_A"));
    oldFile.frames.push_back(frameWith("FOld", 8, "ECU_B"));
    oldFile.frames.push_back(frameWith("FMoveOld", 8, "ECU_MOVE"));
    oldFile.frames.push_back(frameWith("AMB1", 16, ""));
    oldFile.frames.push_back(frameWith("AMB2", 16, ""));
    oldFile.signals.push_back(signalWith("SMod", 3));
    oldFile.signals.push_back(signalWith("SStay", 1));
    oldFile.signals.push_back(signalWith("SMoveOld", 5, 1.0));
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
    newFile.signals.push_back(signalWith("SMoveNew", 5, 2.0));
    newProject.files.push_back(newFile);
    return {oldProject, newProject};
}

}  // namespace

TEST(DiffEngineEndToEndTest, CombinedFixtureCoversEveryCategory) {
    const auto [oldProject, newProject] = combinedFixtures();
    const DiffReport report = DiffEngine{}.diff(oldProject, newProject);

    // Added: FNew + AMB3 + AMB4. Removed: FOld + AMB1 + AMB2.
    EXPECT_EQ(countKind(report, DiffKind::Added, "Frame"), 3);
    EXPECT_EQ(countKind(report, DiffKind::Removed, "Frame"), 3);
    // Moved: plain FMove + SMove-with-factor-change.
    EXPECT_EQ(countKind(report, DiffKind::Moved), 2);
    // Modified: SMod startBit change.
    EXPECT_EQ(countKind(report, DiffKind::Modified, "Signal"), 1);
    // Ambiguous group surfaced, not guessed.
    EXPECT_FALSE(report.diagnostics.empty());
    // Unchanged FStay/SStay produce no entries.
    for (const auto& e : report.entries) {
        EXPECT_NE(e.oldPath, "/FStay");
        EXPECT_NE(e.newPath, "/FStay");
        EXPECT_NE(e.oldPath, "/SStay");
    }
}

TEST(DiffEngineEndToEndTest, IdenticalProjectsProduceEmptyReport) {
    const auto [oldProject, newProject] = combinedFixtures();
    const DiffReport report = DiffEngine{}.diff(oldProject, oldProject);
    EXPECT_TRUE(report.entries.empty());
    EXPECT_TRUE(report.diagnostics.empty());
    (void)newProject;
}

TEST(DiffEngineEndToEndTest, MovedPlusModifiedIsOneEntryInCombinedFixture) {
    const auto [oldProject, newProject] = combinedFixtures();
    const DiffReport report = DiffEngine{}.diff(oldProject, newProject);

    int signalMoved = 0;
    for (const auto& e : report.entries) {
        if (e.kind == DiffKind::Moved && e.elementType == "Signal") {
            ++signalMoved;
            EXPECT_EQ(e.oldPath, "/SMoveOld");
            EXPECT_EQ(e.newPath, "/SMoveNew");
            ASSERT_FALSE(e.fieldDiffs.empty());
        }
    }
    EXPECT_EQ(signalMoved, 1);
}
