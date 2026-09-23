// Unit tests for DiffEngine facade incl. Moved+Modified handling (PAR-135).
#include <gtest/gtest.h>

#include <parsex/diff/diff_engine.hpp>
#include <parsex/model/parsed_project.hpp>

TEST(DiffEngineTest, PlainMoveYieldsOneMovedWithEmptyDiffs) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    Frame oldFrame;
    oldFrame.common.shortName = "FOld";
    oldFrame.length = 8;
    oldFile.frames.push_back(oldFrame);
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    Frame newFrame = oldFrame;
    newFrame.common.shortName = "FNew";
    newFile.frames.push_back(newFrame);
    newProject.files.push_back(newFile);

    // Spot-check both renderers handle the empty-diff Moved entry.
    const DiffReport report = DiffEngine{}.diff(oldProject, newProject);
    ASSERT_EQ(report.entries.size(), 1U);
    EXPECT_EQ(report.entries[0].kind, DiffKind::Moved);
    EXPECT_TRUE(report.entries[0].fieldDiffs.empty());
    EXPECT_NE(report.toText().find("Moved:"), std::string::npos);
    EXPECT_EQ(report.toJson()["payload"]["entries"][0]["kind"], "moved");
}

TEST(DiffEngineTest, MovedAndChangedIsSingleMovedEntryWithDiffs) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    Signal oldSig;
    oldSig.common.shortName = "SOld";
    oldSig.startBit = 3;
    oldSig.bitLength = 8;
    oldSig.factor = 1.0;
    oldFile.signals.push_back(oldSig);
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    Signal newSig = oldSig;
    newSig.common.shortName = "SNew";
    // factor is outside the Signal secondary key (start/len/order/receivers),
    // so the move still correlates and the change rides on the same entry.
    newSig.factor = 2.0;
    newFile.signals.push_back(newSig);
    newProject.files.push_back(newFile);

    const DiffReport report = DiffEngine{}.diff(oldProject, newProject);
    ASSERT_EQ(report.entries.size(), 1U);
    EXPECT_EQ(report.entries[0].kind, DiffKind::Moved);
    ASSERT_FALSE(report.entries[0].fieldDiffs.empty());
    bool sawFactor = false;
    for (const auto& f : report.entries[0].fieldDiffs) {
        if (f.fieldName == "factor") {
            EXPECT_EQ(f.oldValue, "1");
            EXPECT_EQ(f.newValue, "2");
            sawFactor = true;
        }
    }
    EXPECT_TRUE(sawFactor);
    // Spot-check renderers handle the non-empty-diff Moved entry.
    EXPECT_NE(report.toText().find("factor: 1 -> 2"), std::string::npos);
    EXPECT_EQ(report.toJson()["payload"]["entries"][0]["kind"], "moved");
}
