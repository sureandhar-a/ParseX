// Comparison convergence: writing the target side must eliminate the
// comparison result. The writer handles whole-document writes (not granular
// edit application), so full-target write is the supported path here —
// partial edit application beyond that is noted as follow-up, not blocked on.
#include <gtest/gtest.h>

#include <parsex/diff/diff_engine.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/write/write_engine.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

ParsedProject load(const std::filesystem::path& path) {
    ParsedProject project;
    project.files.push_back(Parser{}.parseFile(path));
    return project;
}

// Given A and B: diff(A,B) must be non-empty (sanity), then write B,
// reparse as B2, and diff(B2,B) must be empty (convergence).
void checkReconverge(const std::string& aName, const std::string& bName) {
    const ParsedProject a = load(fixture(aName));
    const ParsedProject b = load(fixture(bName));
    const DiffReport initial = DiffEngine{}.diff(a, b);
    EXPECT_FALSE(initial.empty()) << aName << " vs " << bName << " should differ";
    const std::filesystem::path temp =
        std::filesystem::temp_directory_path() / "parsex_reconverge_tmp.arxml";
    WriteEngine{}.write(b, temp);
    ParsedProject b2;
    b2.files.push_back(Parser{}.parseFile(temp));
    std::error_code ignored;
    std::filesystem::remove(temp, ignored);
    const DiffReport after = DiffEngine{}.diff(b2, b);
    if (!after.empty()) {
        FAIL() << "reconvergence failed for " << bName << ":\n" << after.toText();
    }
}

}  // namespace

TEST(ReconvergeTest, TargetWriteEliminatesComparison) {
    // Pairs already used by comparison tests, known to differ meaningfully.
    // Note: version-only pairs (oldest vs newest with identical content)
    // compare equal since release is metadata, not domain content — excluded.
    checkReconverge("system-4.2.arxml", "release_multiline.arxml");
    checkReconverge("release_multiline.arxml", "system-4.2.arxml");
    checkReconverge("hardening_empty_wellformed.arxml", "hardening_version_oldest.arxml");
    checkReconverge("hardening_empty_wellformed.arxml", "hardening_deep_nesting.arxml");
}
