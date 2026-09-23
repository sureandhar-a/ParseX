// Consolidated move-detection coverage for PAR-117 Done-when criteria
// (PAR-126): moved, unrelated, and ambiguous-candidates cases through the
// full PAR-116 matching pass followed by the move-detection pass.
#include <gtest/gtest.h>

#include <parsex/diff/diff_match.hpp>
#include <parsex/diff/diff_moves.hpp>
#include <parsex/model/parsed_project.hpp>

namespace {

Frame frameWith(const std::string& name, std::uint32_t length) {
    Frame f;
    f.common.shortName = name;
    f.length = length;
    return f;
}

int countKind(const DiffReport& report, DiffKind kind) {
    int n = 0;
    for (const auto& e : report.entries) {
        if (e.kind == kind) {
            ++n;
        }
    }
    return n;
}

}  // namespace

// Test 1: Frame moved to a different package (path changed), length unchanged.
TEST(MoveDetectionTest, MovedFrameReportsSingleMovedEntry) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    oldFile.frames.push_back(frameWith("FOld", 8));
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    newFile.frames.push_back(frameWith("FNew", 8));
    newProject.files.push_back(newFile);

    DiffReport matched = matchParsedProjects(oldProject, newProject);
    DiffReport result = applyMoveDetection(oldProject, newProject, std::move(matched));

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].kind, DiffKind::Moved);
    EXPECT_EQ(result.entries[0].oldPath, "/FOld");
    EXPECT_EQ(result.entries[0].newPath, "/FNew");
    EXPECT_TRUE(result.diagnostics.empty());
}

// Test 2: genuinely unrelated Removed + Added with different keys.
TEST(MoveDetectionTest, UnrelatedPairStaysRemovedAndAdded) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    oldFile.frames.push_back(frameWith("FOld", 8));
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    newFile.frames.push_back(frameWith("FNew", 4));
    newProject.files.push_back(newFile);

    DiffReport matched = matchParsedProjects(oldProject, newProject);
    DiffReport result = applyMoveDetection(oldProject, newProject, std::move(matched));

    EXPECT_EQ(countKind(result, DiffKind::Moved), 0);
    EXPECT_EQ(countKind(result, DiffKind::Removed), 1);
    EXPECT_EQ(countKind(result, DiffKind::Added), 1);
}

// Test 3: ambiguous — keys don't pair 1:1, report uncertainty.
TEST(MoveDetectionTest, AmbiguousCandidatesProduceDiagnosticNotMove) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    oldFile.frames.push_back(frameWith("FOld1", 8));
    oldFile.frames.push_back(frameWith("FOld2", 8));
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    newFile.frames.push_back(frameWith("FNew1", 8));
    newFile.frames.push_back(frameWith("FNew2", 8));
    newProject.files.push_back(newFile);

    DiffReport matched = matchParsedProjects(oldProject, newProject);
    DiffReport result = applyMoveDetection(oldProject, newProject, std::move(matched));

    EXPECT_EQ(countKind(result, DiffKind::Moved), 0);
    EXPECT_FALSE(result.diagnostics.empty());
}
