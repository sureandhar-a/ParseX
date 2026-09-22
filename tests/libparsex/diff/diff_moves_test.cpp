// Unit tests for secondary-key move correlation (PAR-124) and ambiguous-match
// diagnostics (PAR-125).
#include <gtest/gtest.h>

#include <parsex/diff/diff_match.hpp>
#include <parsex/diff/diff_moves.hpp>
#include <parsex/model/parsed_project.hpp>

namespace {

Frame frameNamed(const std::string& name, std::uint32_t length) {
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

TEST(MoveCorrelationTest, RenamedFrameWithSameKeyBecomesMoved) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    oldFile.frames.push_back(frameNamed("FOld", 8));
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    newFile.frames.push_back(frameNamed("FNew", 8));
    newProject.files.push_back(newFile);

    DiffReport matched = matchParsedProjects(oldProject, newProject);
    ASSERT_EQ(countKind(matched, DiffKind::Removed), 1);
    ASSERT_EQ(countKind(matched, DiffKind::Added), 1);

    DiffReport result = applyMoveDetection(oldProject, newProject, std::move(matched));
    EXPECT_EQ(countKind(result, DiffKind::Moved), 1);
    EXPECT_EQ(countKind(result, DiffKind::Removed), 0);
    EXPECT_EQ(countKind(result, DiffKind::Added), 0);
    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].oldPath, "/FOld");
    EXPECT_EQ(result.entries[0].newPath, "/FNew");
    EXPECT_EQ(result.entries[0].elementType, "Frame");
}

TEST(MoveCorrelationTest, UnrelatedFramesStayRemovedAndAdded) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    oldFile.frames.push_back(frameNamed("FOld", 8));
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    newFile.frames.push_back(frameNamed("FNew", 4));
    newProject.files.push_back(newFile);

    DiffReport matched = matchParsedProjects(oldProject, newProject);
    DiffReport result = applyMoveDetection(oldProject, newProject, std::move(matched));
    EXPECT_EQ(countKind(result, DiffKind::Moved), 0);
    EXPECT_EQ(countKind(result, DiffKind::Removed), 1);
    EXPECT_EQ(countKind(result, DiffKind::Added), 1);
}

TEST(AmbiguousMoveTest, SharedKeyLeavesAllAndEmitsOneDiagnostic) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    oldFile.frames.push_back(frameNamed("FOld1", 8));
    oldFile.frames.push_back(frameNamed("FOld2", 8));
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    newFile.frames.push_back(frameNamed("FNew1", 8));
    newFile.frames.push_back(frameNamed("FNew2", 8));
    newProject.files.push_back(newFile);

    DiffReport matched = matchParsedProjects(oldProject, newProject);
    DiffReport result = applyMoveDetection(oldProject, newProject, std::move(matched));

    EXPECT_EQ(countKind(result, DiffKind::Moved), 0);
    EXPECT_EQ(countKind(result, DiffKind::Removed), 2);
    EXPECT_EQ(countKind(result, DiffKind::Added), 2);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].elementType, "Frame");
    EXPECT_NE(result.diagnostics[0].message.find("4 candidates"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("no move inferred"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("/FOld1"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("/FNew2"), std::string::npos);
}

TEST(AmbiguousMoveTest, CleanPairProducesZeroDiagnostics) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    oldFile.frames.push_back(frameNamed("FOld", 8));
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    newFile.frames.push_back(frameNamed("FNew", 8));
    newProject.files.push_back(newFile);

    DiffReport matched = matchParsedProjects(oldProject, newProject);
    DiffReport result = applyMoveDetection(oldProject, newProject, std::move(matched));
    EXPECT_EQ(countKind(result, DiffKind::Moved), 1);
    EXPECT_TRUE(result.diagnostics.empty());
}
