// Hand-authored edge-case corpus: inputs a fuzzer rarely finds on its own.
// Each fixture below probes one specific edge; keep them small and do not
// delete as redundant — the comment on each .arxml file explains its purpose.
#include <gtest/gtest.h>

#include <parsex/parser/parser.hpp>
#include <parsex/validator/validator.hpp>

#include <filesystem>

namespace {

std::filesystem::path fixtureDir() {
    return std::filesystem::path(PARSEX_FIXTURE_DIR);
}

struct Counts {
    std::size_t clusters = 0;
    std::size_t ecus = 0;
};

Counts checkSemanticPass(const std::string& name, const std::string& expectedRelease) {
    const Parser parser;
    ParsedFile file = parser.parseFile(fixtureDir() / name);
    EXPECT_EQ(file.autosarRelease, expectedRelease) << name;
    ParsedProject project;
    project.files.push_back(file);
    Validator validator;
    EXPECT_TRUE(validator.validateReferences(project).errors.empty()) << name;
    EXPECT_TRUE(validator.validateUniqueness(project).errors.empty()) << name;
    EXPECT_TRUE(validator.validateCanSemantics(project).errors.empty()) << name;
    return {.clusters = project.files.front().clusters.size(),
            .ecus = project.files.front().ecuInstances.size()};
}

}  // namespace

// Empty-but-well-formed: zero objects must still validate semantically.
TEST(HardeningEdges, EmptyWellformed) {
    const Counts counts = checkSemanticPass("hardening_empty_wellformed.arxml", "4.2.2");
    EXPECT_EQ(counts.clusters, 0U);
    EXPECT_EQ(counts.ecus, 0U);
}

// Deep nesting (12 package levels): recursion and spans must hold.
TEST(HardeningEdges, DeepNesting) {
    const Counts counts = checkSemanticPass("hardening_deep_nesting.arxml", "4.2.2");
    EXPECT_EQ(counts.clusters, 1U);
}

// Oldest release boundary: must resolve, not reject.
TEST(HardeningEdges, VersionOldest) {
    const Counts counts = checkSemanticPass("hardening_version_oldest.arxml", "4.2.2");
    EXPECT_EQ(counts.clusters, 1U);
}

// Newest release boundary: must resolve, not reject.
TEST(HardeningEdges, VersionNewest) {
    const Counts counts = checkSemanticPass("hardening_version_newest.arxml", "R21-11");
    EXPECT_EQ(counts.clusters, 1U);
}

// Maximum-length identifiers (128 chars): no truncation, joins still work.
TEST(HardeningEdges, LongNames) {
    const Counts counts = checkSemanticPass("hardening_long_names.arxml", "4.2.2");
    EXPECT_EQ(counts.clusters, 1U);
    EXPECT_EQ(counts.ecus, 1U);
    // Long ECU without controllers still warns at parse time (best-effort).
    const Parser parser;
    const ParsedFile file = parser.parseFile(fixtureDir() / "hardening_long_names.arxml");
    EXPECT_EQ(file.warnings.size(), 1U);
}
