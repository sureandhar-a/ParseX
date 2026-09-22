// Fixture-level tests for PAR-116 Done-when criteria (PAR-123): identical
// projects, single Added, single Removed, and combined mixed-type cases.
#include <gtest/gtest.h>

#include <parsex/diff/diff_match.hpp>
#include <parsex/model/parsed_project.hpp>

namespace {

ParsedFile makeFullFile(const std::string& suffix) {
    ParsedFile file;
    Cluster c;
    c.common.shortName = "CL" + suffix;
    file.clusters.push_back(c);
    EcuInstance e;
    e.common.shortName = "ECU" + suffix;
    file.ecuInstances.push_back(e);
    Frame f;
    f.common.shortName = "F" + suffix;
    file.frames.push_back(f);
    Pdu p;
    p.common.shortName = "P" + suffix;
    file.pdus.push_back(p);
    Signal s;
    s.common.shortName = "S" + suffix;
    file.signals.push_back(s);
    SignalGroup g;
    g.common.shortName = "G" + suffix;
    file.signalGroups.push_back(g);
    return file;
}

int countKind(const DiffReport& report, DiffKind kind, const std::string& type = "") {
    int count = 0;
    for (const auto& entry : report.entries) {
        if (entry.kind == kind && (type.empty() || entry.elementType == type)) {
            ++count;
        }
    }
    return count;
}

}  // namespace

// Fixture A: identical projects -> only empty-fieldDiffs Modified entries,
// count equals total elements across all six types.
TEST(PathMatchingFixturesTest, IdenticalProjectsProduceOnlyMatchedPlaceholders) {
    ParsedProject oldProject;
    oldProject.files.push_back(makeFullFile("1"));
    ParsedProject newProject;
    newProject.files.push_back(makeFullFile("1"));

    const DiffReport report = matchParsedProjects(oldProject, newProject);

    EXPECT_EQ(report.entries.size(), 6U);
    EXPECT_EQ(countKind(report, DiffKind::Modified), 6);
    EXPECT_EQ(countKind(report, DiffKind::Added), 0);
    EXPECT_EQ(countKind(report, DiffKind::Removed), 0);
    for (const auto& entry : report.entries) {
        EXPECT_TRUE(entry.fieldDiffs.empty());
    }
}

// Fixture B: new project has one extra Frame.
TEST(PathMatchingFixturesTest, ExtraFrameInNewIsSingleAdded) {
    ParsedProject oldProject;
    oldProject.files.push_back(makeFullFile("1"));
    ParsedProject newProject;
    newProject.files.push_back(makeFullFile("1"));
    Frame extra;
    extra.common.shortName = "FExtra";
    newProject.files.back().frames.push_back(extra);

    const DiffReport report = matchParsedProjects(oldProject, newProject);

    EXPECT_EQ(countKind(report, DiffKind::Added, "Frame"), 1);
    EXPECT_EQ(countKind(report, DiffKind::Removed), 0);
    bool found = false;
    for (const auto& entry : report.entries) {
        if (entry.kind == DiffKind::Added && entry.elementType == "Frame") {
            EXPECT_EQ(entry.newPath, "/FExtra");
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

// Fixture C: old project has one Frame absent from new.
TEST(PathMatchingFixturesTest, MissingFrameInNewIsSingleRemoved) {
    ParsedProject oldProject;
    oldProject.files.push_back(makeFullFile("1"));
    Frame extra;
    extra.common.shortName = "FOld";
    oldProject.files.back().frames.push_back(extra);
    ParsedProject newProject;
    newProject.files.push_back(makeFullFile("1"));

    const DiffReport report = matchParsedProjects(oldProject, newProject);

    EXPECT_EQ(countKind(report, DiffKind::Removed, "Frame"), 1);
    EXPECT_EQ(countKind(report, DiffKind::Added), 0);
    bool found = false;
    for (const auto& entry : report.entries) {
        if (entry.kind == DiffKind::Removed && entry.elementType == "Frame") {
            EXPECT_EQ(entry.oldPath, "/FOld");
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

// Fixture D: combined — added Signal, removed Frame, two matched Pdus;
// nothing bleeds across domain types.
TEST(PathMatchingFixturesTest, CombinedCountsAndPathsAreExact) {
    ParsedProject oldProject;
    ParsedFile oldFile;
    Frame removed;
    removed.common.shortName = "FOld";
    oldFile.frames.push_back(removed);
    Pdu p1;
    p1.common.shortName = "P1";
    Pdu p2;
    p2.common.shortName = "P2";
    oldFile.pdus.push_back(p1);
    oldFile.pdus.push_back(p2);
    oldProject.files.push_back(oldFile);

    ParsedProject newProject;
    ParsedFile newFile;
    Signal added;
    added.common.shortName = "SNew";
    newFile.signals.push_back(added);
    Pdu q1;
    q1.common.shortName = "P1";
    Pdu q2;
    q2.common.shortName = "P2";
    newFile.pdus.push_back(q1);
    newFile.pdus.push_back(q2);
    newProject.files.push_back(newFile);

    const DiffReport report = matchParsedProjects(oldProject, newProject);

    EXPECT_EQ(countKind(report, DiffKind::Added), 1);
    EXPECT_EQ(countKind(report, DiffKind::Removed), 1);
    EXPECT_EQ(countKind(report, DiffKind::Modified), 2);
    EXPECT_EQ(countKind(report, DiffKind::Added, "Signal"), 1);
    EXPECT_EQ(countKind(report, DiffKind::Removed, "Frame"), 1);
    EXPECT_EQ(countKind(report, DiffKind::Added, "Frame"), 0);
    EXPECT_EQ(countKind(report, DiffKind::Removed, "Signal"), 0);
    EXPECT_EQ(countKind(report, DiffKind::Modified, "Pdu"), 2);
}
