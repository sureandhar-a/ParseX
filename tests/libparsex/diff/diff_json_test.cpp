// Unit tests for structured JSON renderer (PAR-133).
#include <gtest/gtest.h>

#include <parsex/diff/diff_report.hpp>

namespace {

DiffReport sampleReport() {
    DiffReport report;
    for (const auto kind : {DiffKind::Added, DiffKind::Removed, DiffKind::Moved, DiffKind::Modified}) {
        DiffEntry e;
        e.kind = kind;
        e.elementType = "Frame";
        e.oldPath = kind == DiffKind::Added ? "" : "/F/Old";
        e.newPath = kind == DiffKind::Removed ? "" : "/F/New";
        if (kind == DiffKind::Modified) {
            e.newPath = "/F/Old";
            e.fieldDiffs.push_back({.fieldName = "length", .oldValue = "8", .newValue = "64"});
        }
        report.entries.push_back(e);
    }
    report.diagnostics.push_back({.message = "note", .elementType = "Frame"});
    return report;
}

}  // namespace

TEST(DiffJsonTest, ShapeHasExpectedKeysAndValues) {
    const nlohmann::json j = sampleReport().toJson();

    ASSERT_TRUE(j.contains("entries"));
    ASSERT_TRUE(j.contains("diagnostics"));
    ASSERT_EQ(j["entries"].size(), 4U);

    const nlohmann::json added = j["entries"][0];
    EXPECT_EQ(added["kind"], "added");
    EXPECT_EQ(added["elementType"], "Frame");
    EXPECT_EQ(added["oldPath"], "");
    EXPECT_EQ(added["newPath"], "/F/New");
    EXPECT_TRUE(added.contains("fieldDiffs"));

    const nlohmann::json modified = j["entries"][3];
    EXPECT_EQ(modified["kind"], "modified");
    ASSERT_EQ(modified["fieldDiffs"].size(), 1U);
    EXPECT_EQ(modified["fieldDiffs"][0]["field"], "length");
    EXPECT_EQ(modified["fieldDiffs"][0]["oldValue"], "8");
    EXPECT_EQ(modified["fieldDiffs"][0]["newValue"], "64");
}

TEST(DiffJsonTest, RoundTripPreservesAllFields) {
    const DiffReport original = sampleReport();
    const DiffReport restored = DiffReport::fromJson(original.toJson());

    ASSERT_EQ(restored.entries.size(), original.entries.size());
    for (std::size_t i = 0; i < original.entries.size(); ++i) {
        EXPECT_EQ(restored.entries[i].kind, original.entries[i].kind);
        EXPECT_EQ(restored.entries[i].elementType, original.entries[i].elementType);
        EXPECT_EQ(restored.entries[i].oldPath, original.entries[i].oldPath);
        EXPECT_EQ(restored.entries[i].newPath, original.entries[i].newPath);
        ASSERT_EQ(restored.entries[i].fieldDiffs.size(), original.entries[i].fieldDiffs.size());
        for (std::size_t k = 0; k < original.entries[i].fieldDiffs.size(); ++k) {
            EXPECT_EQ(restored.entries[i].fieldDiffs[k].fieldName,
                      original.entries[i].fieldDiffs[k].fieldName);
            EXPECT_EQ(restored.entries[i].fieldDiffs[k].oldValue,
                      original.entries[i].fieldDiffs[k].oldValue);
            EXPECT_EQ(restored.entries[i].fieldDiffs[k].newValue,
                      original.entries[i].fieldDiffs[k].newValue);
        }
    }
    ASSERT_EQ(restored.diagnostics.size(), original.diagnostics.size());
    EXPECT_EQ(restored.diagnostics[0].message, "note");
}
