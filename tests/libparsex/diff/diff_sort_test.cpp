// Unit tests for deterministic sort pass (PAR-131).
#include <gtest/gtest.h>

#include <parsex/diff/diff_match.hpp>
#include <parsex/diff/diff_report.hpp>
#include <parsex/model/parsed_project.hpp>

namespace {

DiffEntry entry(DiffKind kind, const std::string& type, const std::string& oldPath,
                const std::string& newPath) {
    DiffEntry e;
    e.kind = kind;
    e.elementType = type;
    e.oldPath = oldPath;
    e.newPath = newPath;
    return e;
}

}  // namespace

TEST(SortDeterministicallyTest, ScrambledEntriesSortIntoPathOrder) {
    DiffReport report;
    report.entries.push_back(entry(DiffKind::Added, "Signal", "", "/S/Z"));
    report.entries.push_back(entry(DiffKind::Removed, "Frame", "/F/A", ""));
    report.entries.push_back(entry(DiffKind::Modified, "Pdu", "/P/M", "/P/M"));
    report.entries.push_back(entry(DiffKind::Moved, "Frame", "/F/Old", "/F/B"));

    report.sortDeterministically();

    ASSERT_EQ(report.entries.size(), 4U);
    // Keys: /S/Z, /F/A, /P/M, /F/B (Moved by newPath) -> /F/A, /F/B, /P/M, /S/Z
    EXPECT_EQ(report.entries[0].oldPath, "/F/A");
    EXPECT_EQ(report.entries[1].newPath, "/F/B");
    EXPECT_EQ(report.entries[1].kind, DiffKind::Moved);
    EXPECT_EQ(report.entries[2].newPath, "/P/M");
    EXPECT_EQ(report.entries[3].newPath, "/S/Z");
}

TEST(SortDeterministicallyTest, PipelineTwiceYieldsIdenticalOrder) {
    auto build = [] {
        ParsedProject oldP;
        ParsedFile of;
        for (const char* n : {"F1", "F2", "F3", "F4", "F5"}) {
            Frame f;
            f.common.shortName = n;
            of.frames.push_back(f);
        }
        oldP.files.push_back(of);
        ParsedProject newP;
        ParsedFile nf;
        for (const char* n : {"F3", "F4", "F5", "F6", "F7"}) {
            Frame f;
            f.common.shortName = n;
            nf.frames.push_back(f);
        }
        newP.files.push_back(nf);
        DiffReport r = matchParsedProjects(oldP, newP);
        r.sortDeterministically();
        return r;
    };

    const DiffReport first = build();
    const DiffReport second = build();
    ASSERT_EQ(first.entries.size(), second.entries.size());
    for (std::size_t i = 0; i < first.entries.size(); ++i) {
        EXPECT_EQ(first.entries[i].oldPath, second.entries[i].oldPath);
        EXPECT_EQ(first.entries[i].newPath, second.entries[i].newPath);
        EXPECT_EQ(first.entries[i].kind, second.entries[i].kind);
    }
}
