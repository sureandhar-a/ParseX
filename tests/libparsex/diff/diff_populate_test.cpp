// Integration tests for field-diff wiring (PAR-129).
#include <gtest/gtest.h>

#include <parsex/diff/diff_match.hpp>
#include <parsex/diff/diff_populate.hpp>
#include <parsex/model/parsed_project.hpp>

TEST(PopulateFieldDiffsTest, SingleSignalChangeYieldsOneModifiedEntry) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    Signal oldSig;
    oldSig.common.shortName = "S1";
    oldSig.startBit = 3;
    oldSig.bitLength = 8;
    oldFile.signals.push_back(oldSig);
    Signal other;
    other.common.shortName = "S2";
    oldFile.signals.push_back(other);
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    Signal newSig = oldSig;
    newSig.startBit = 7;
    newFile.signals.push_back(newSig);
    newFile.signals.push_back(other);
    newProject.files.push_back(newFile);

    DiffReport matched = matchParsedProjects(oldProject, newProject);
    DiffReport result = populateFieldDiffs(oldProject, newProject, std::move(matched));

    ASSERT_EQ(result.entries.size(), 1U);
    EXPECT_EQ(result.entries[0].kind, DiffKind::Modified);
    EXPECT_EQ(result.entries[0].elementType, "Signal");
    ASSERT_EQ(result.entries[0].fieldDiffs.size(), 1U);
    EXPECT_EQ(result.entries[0].fieldDiffs[0].fieldName, "startBit");
}

TEST(PopulateFieldDiffsTest, IdenticalProjectsYieldZeroEntries) {
    ParsedProject project;
    ParsedFile file;
    Signal sig;
    sig.common.shortName = "S1";
    file.signals.push_back(sig);
    Frame frame;
    frame.common.shortName = "F1";
    file.frames.push_back(frame);
    project.files.push_back(file);

    DiffReport matched = matchParsedProjects(project, project);
    ASSERT_FALSE(matched.entries.empty());  // placeholders exist before wiring
    DiffReport result = populateFieldDiffs(project, project, std::move(matched));
    EXPECT_TRUE(result.entries.empty());
}
