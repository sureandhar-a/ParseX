// Unit tests for path-based matching (PAR-122): indexByPath, matchByPath
// per-type behavior, and whole-project six-type merge.
#include <gtest/gtest.h>

#include <parsex/diff/diff_match.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/parsed_project.hpp>

namespace {

Frame frameNamed(const std::string& name) {
    Frame frame;
    frame.common.shortName = name;
    return frame;
}

int countKind(const DiffReport& report, DiffKind kind) {
    int count = 0;
    for (const auto& entry : report.entries) {
        if (entry.kind == kind) {
            ++count;
        }
    }
    return count;
}

ParsedFile fileWith(const std::string& cluster, const std::string& ecu, const std::string& frame,
                    const std::string& pdu, const std::string& signal,
                    const std::string& group) {
    ParsedFile file;
    if (!cluster.empty()) {
        Cluster c;
        c.common.shortName = cluster;
        file.clusters.push_back(std::move(c));
    }
    if (!ecu.empty()) {
        EcuInstance e;
        e.common.shortName = ecu;
        file.ecuInstances.push_back(std::move(e));
    }
    if (!frame.empty()) {
        file.frames.push_back(frameNamed(frame));
    }
    if (!pdu.empty()) {
        Pdu p;
        p.common.shortName = pdu;
        file.pdus.push_back(std::move(p));
    }
    if (!signal.empty()) {
        Signal s;
        s.common.shortName = signal;
        file.signals.push_back(std::move(s));
    }
    if (!group.empty()) {
        SignalGroup g;
        g.common.shortName = group;
        file.signalGroups.push_back(std::move(g));
    }
    return file;
}

}  // namespace

TEST(DiffMatchTest, IdenticalFramesProduceOnlyModifiedPlaceholders) {
    const std::vector<Frame> oldFrames = {frameNamed("A"), frameNamed("B"), frameNamed("C")};
    const std::vector<Frame> newFrames = {frameNamed("A"), frameNamed("B"), frameNamed("C")};

    DiffReport report =
        matchByPath(indexByPath(oldFrames), indexByPath(newFrames), "Frame");

    EXPECT_EQ(report.entries.size(), 3U);
    EXPECT_EQ(countKind(report, DiffKind::Modified), 3);
    EXPECT_EQ(countKind(report, DiffKind::Added), 0);
    EXPECT_EQ(countKind(report, DiffKind::Removed), 0);
    for (const auto& entry : report.entries) {
        EXPECT_TRUE(entry.fieldDiffs.empty());
        EXPECT_EQ(entry.oldPath, entry.newPath);
        EXPECT_EQ(entry.elementType, "Frame");
    }
}

TEST(DiffMatchTest, AddedRemovedAndMatched) {
    const std::vector<Frame> oldFrames = {frameNamed("A"), frameNamed("B")};
    const std::vector<Frame> newFrames = {frameNamed("A"), frameNamed("C")};

    DiffReport report =
        matchByPath(indexByPath(oldFrames), indexByPath(newFrames), "Frame");

    EXPECT_EQ(report.entries.size(), 3U);
    EXPECT_EQ(countKind(report, DiffKind::Modified), 1);
    EXPECT_EQ(countKind(report, DiffKind::Added), 1);
    EXPECT_EQ(countKind(report, DiffKind::Removed), 1);

    bool sawAddedC = false;
    bool sawRemovedB = false;
    bool sawModifiedA = false;
    for (const auto& entry : report.entries) {
        if (entry.kind == DiffKind::Added) {
            EXPECT_EQ(entry.newPath, "/C");
            sawAddedC = true;
        } else if (entry.kind == DiffKind::Removed) {
            EXPECT_EQ(entry.oldPath, "/B");
            sawRemovedB = true;
        } else {
            EXPECT_EQ(entry.oldPath, "/A");
            EXPECT_EQ(entry.newPath, "/A");
            sawModifiedA = true;
        }
    }
    EXPECT_TRUE(sawAddedC);
    EXPECT_TRUE(sawRemovedB);
    EXPECT_TRUE(sawModifiedA);
}

TEST(DiffMatchTest, SixTypeMergeProducesExpectedTotals) {
    ParsedProject oldProject;
    oldProject.files.push_back(fileWith("CL", "ECU", "FShared", "PShared", "SShared", "GShared"));
    oldProject.files.back().frames.push_back(frameNamed("FOldOnly"));

    ParsedProject newProject;
    newProject.files.push_back(fileWith("CL", "ECU", "FShared", "PShared", "SShared", "GShared"));
    newProject.files.back().frames.push_back(frameNamed("FNewOnly"));

    const DiffReport report = matchParsedProjects(oldProject, newProject);

    // 6 shared elements -> 6 Modified; frames add 1 Removed + 1 Added.
    EXPECT_EQ(report.entries.size(), 8U);
    EXPECT_EQ(countKind(report, DiffKind::Modified), 6);
    EXPECT_EQ(countKind(report, DiffKind::Added), 1);
    EXPECT_EQ(countKind(report, DiffKind::Removed), 1);
}
