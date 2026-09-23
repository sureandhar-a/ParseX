// Unit tests for human-readable text renderer (PAR-132).
#include <gtest/gtest.h>

#include <parsex/diff/diff_report.hpp>

namespace {

DiffEntry added(const std::string& type, const std::string& path) {
    DiffEntry e;
    e.kind = DiffKind::Added;
    e.elementType = type;
    e.newPath = path;
    return e;
}

DiffEntry removed(const std::string& type, const std::string& path) {
    DiffEntry e;
    e.kind = DiffKind::Removed;
    e.elementType = type;
    e.oldPath = path;
    return e;
}

}  // namespace

TEST(DiffTextTest, SectionsLabeledAndEmptyOnesSkipped) {
    DiffReport report;
    report.entries.push_back(added("Frame", "/F/New"));
    report.entries.push_back(removed("Signal", "/S/Old"));
    DiffEntry mod;
    mod.kind = DiffKind::Modified;
    mod.elementType = "Signal";
    mod.oldPath = "/S/M";
    mod.newPath = "/S/M";
    mod.fieldDiffs.push_back({.fieldName = "startBit", .oldValue = "3", .newValue = "7"});
    report.entries.push_back(mod);

    const std::string text = report.toText();
    EXPECT_NE(text.find("Added:"), std::string::npos);
    EXPECT_NE(text.find("Removed:"), std::string::npos);
    EXPECT_NE(text.find("Modified:"), std::string::npos);
    EXPECT_EQ(text.find("Moved:"), std::string::npos);  // zero entries -> no header
    EXPECT_NE(text.find("/F/New"), std::string::npos);
    EXPECT_NE(text.find("startBit: 3 -> 7"), std::string::npos);
}

TEST(DiffTextTest, MovedWithChangesRendersPathsAndFieldsTogether) {
    DiffReport report;
    DiffEntry moved;
    moved.kind = DiffKind::Moved;
    moved.elementType = "Frame";
    moved.oldPath = "/Pkg/Old";
    moved.newPath = "/Pkg/New";
    moved.fieldDiffs.push_back({.fieldName = "length", .oldValue = "8", .newValue = "64"});
    report.entries.push_back(moved);

    const std::string text = report.toText();
    EXPECT_NE(text.find("Moved:"), std::string::npos);
    EXPECT_NE(text.find("/Pkg/Old -> /Pkg/New"), std::string::npos);
    EXPECT_NE(text.find("length: 8 -> 64"), std::string::npos);
}

TEST(DiffTextTest, DiagnosticsSectionListedWhenPresent) {
    DiffReport report;
    report.entries.push_back(added("Frame", "/F/New"));
    report.diagnostics.push_back({.message = "2 candidates shared secondary key K, no move inferred",
                                  .elementType = "Frame"});

    const std::string text = report.toText();
    EXPECT_NE(text.find("Diagnostics:"), std::string::npos);
    EXPECT_NE(text.find("no move inferred"), std::string::npos);
}
