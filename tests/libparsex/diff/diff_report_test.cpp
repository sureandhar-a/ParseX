// Unit tests for DiffKind/DiffEntry/DiffReport plain types (PAR-121):
// construction per kind, merge() order preservation, debug output.
#include <gtest/gtest.h>

#include <sstream>

#include <parsex/diff/diff_report.hpp>

namespace {

DiffEntry makeEntry(DiffKind kind, const std::string& type, const std::string& oldPath,
                    const std::string& newPath) {
    DiffEntry entry;
    entry.kind = kind;
    entry.elementType = type;
    entry.oldPath = oldPath;
    entry.newPath = newPath;
    return entry;
}

}  // namespace

TEST(DiffReportTypesTest, EachKindConstructsAndHoldsValues) {
    const DiffEntry added = makeEntry(DiffKind::Added, "Frame", "", "/Pkg/NewFrame");
    EXPECT_EQ(added.kind, DiffKind::Added);
    EXPECT_EQ(added.elementType, "Frame");
    EXPECT_EQ(added.newPath, "/Pkg/NewFrame");
    EXPECT_TRUE(added.fieldDiffs.empty());

    const DiffEntry removed = makeEntry(DiffKind::Removed, "Signal", "/Pkg/OldSig", "");
    EXPECT_EQ(removed.kind, DiffKind::Removed);
    EXPECT_EQ(removed.oldPath, "/Pkg/OldSig");

    const DiffEntry moved = makeEntry(DiffKind::Moved, "Pdu", "/Pkg/Old", "/Other/New");
    EXPECT_EQ(moved.kind, DiffKind::Moved);
    EXPECT_EQ(moved.oldPath, "/Pkg/Old");
    EXPECT_EQ(moved.newPath, "/Other/New");

    const DiffEntry modified = makeEntry(DiffKind::Modified, "Signal", "/Pkg/S", "/Pkg/S");
    EXPECT_EQ(modified.kind, DiffKind::Modified);
    EXPECT_EQ(modified.oldPath, "/Pkg/S");
    EXPECT_EQ(modified.newPath, "/Pkg/S");
}

TEST(DiffReportTypesTest, MergeAppendsPreservingRelativeOrder) {
    DiffReport first;
    first.entries.push_back(makeEntry(DiffKind::Added, "Frame", "", "/Pkg/A"));
    first.entries.push_back(makeEntry(DiffKind::Removed, "Frame", "/Pkg/B", ""));

    DiffReport second;
    second.entries.push_back(makeEntry(DiffKind::Moved, "Pdu", "/Pkg/C", "/Pkg/D"));
    second.entries.push_back(makeEntry(DiffKind::Modified, "Signal", "/Pkg/E", "/Pkg/E"));
    second.entries.push_back(makeEntry(DiffKind::Added, "Signal", "", "/Pkg/F"));

    first.merge(std::move(second));

    ASSERT_EQ(first.entries.size(), 5U);
    EXPECT_EQ(first.entries[0].newPath, "/Pkg/A");
    EXPECT_EQ(first.entries[1].oldPath, "/Pkg/B");
    EXPECT_EQ(first.entries[2].kind, DiffKind::Moved);
    EXPECT_EQ(first.entries[3].kind, DiffKind::Modified);
    EXPECT_EQ(first.entries[4].newPath, "/Pkg/F");
    // Moved-from report is drained, not duplicated.
    EXPECT_TRUE(second.entries.empty());
}

TEST(DiffReportTypesTest, DebugStringMentionsKindsAndCounts) {
    DiffReport report;
    report.entries.push_back(makeEntry(DiffKind::Added, "Frame", "", "/Pkg/A"));
    report.entries.push_back(makeEntry(DiffKind::Removed, "Signal", "/Pkg/B", ""));

    const std::string debug = report.toDebugString();
    EXPECT_NE(debug.find("2 entries"), std::string::npos);
    EXPECT_NE(debug.find("Added"), std::string::npos);
    EXPECT_NE(debug.find("Removed"), std::string::npos);

    std::ostringstream oss;
    oss << DiffKind::Modified;
    EXPECT_EQ(oss.str(), "Modified");
}
