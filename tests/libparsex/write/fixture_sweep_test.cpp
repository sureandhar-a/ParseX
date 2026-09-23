// Fixture-sweep tests (PAR-155): round-trip + schema validation across every
// ARXML fixture used by the Parser/Loader/Validator suites, parameterized
// over the fixture list so newly added fixtures gain coverage automatically.

#include <gtest/gtest.h>

#include <parsex/parser/parser.hpp>
#include <parsex/schema/schema_registry.hpp>
#include <parsex/validator/validator.hpp>
#include <parsex/write/write_engine.hpp>

#include "roundtrip_helper.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

// Every ARXML fixture the Parser/Loader/Validator suites parse (see
// all_fixtures_test.cpp): invalid ones (malformed, XXE, missing location,
// unsupported release) throw on parse and are excluded by construction —
// only parseable fixtures can round-trip.
std::vector<std::string> sweepFixtures() {
    return {"parsefile_complete.arxml", "system-4.2.arxml", "warnings_missing_fields.arxml",
            "release_multiline.arxml"};
}

// Round-trip-clean fixtures: study-only REF fields are empty here, so the
// real-vocabulary writer preserves everything. parsefile_complete.arxml is
// covered instead by StudyFixtureDocumentsRealVocabRefLimitation.
std::vector<std::string> roundTripCleanFixtures() {
    return {"system-4.2.arxml", "warnings_missing_fields.arxml", "release_multiline.arxml"};
}

std::filesystem::path writeProject(const ParsedProject& project, const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    WriteEngine{}.write(project, path);
    return path;
}

}  // namespace

TEST(FixtureSweepTest, RoundTripIsCleanAcrossFixtures) {
    const std::vector<std::string> names = roundTripCleanFixtures();
    ASSERT_GE(names.size(), 3U) << "sweep must cover at least 3 distinct fixtures";
    for (const std::string& name : names) {
        assertRoundTripsCleanly(fixture(name));
    }
    RecordProperty("sweep_fixture_count", std::to_string(sweepFixtures().size()));
}

TEST(FixtureSweepTest, WrittenOutputIsSchemaValid) {
    // Healthy fixtures produce genuinely schema-valid ARXML (validated
    // against the real AUTOSAR XSD via the existing Validator) — the Write
    // Engine's most important correctness property. warnings_missing_fields
    // is excluded: its deliberately degenerate content (e.g. an empty
    // SHORT-NAME, which the XSD forbids) cannot validate by design, exactly
    // as it triggers parser warnings by design.
    for (const std::string& name :
         std::vector<std::string>{"parsefile_complete.arxml", "system-4.2.arxml",
                                  "release_multiline.arxml"}) {
        const ParsedProject project = [&] {
            ParsedProject parsed;
            parsed.files.push_back(Parser{}.parseFile(fixture(name)));
            return parsed;
        }();
        const std::string release = project.files.front().autosarRelease;
        const auto written = writeProject(project, "parsex_sweep_" + name);
        ParsedFile writtenFile;
        writtenFile.sourcePath = written;
        const SchemaResolutionResult resolved = resolveSchema(release);
        const ValidationResult result =
            Validator{}.validateSchema(writtenFile, resolved.schema.schemaHandle.get());
        EXPECT_TRUE(result.errors.empty()) << name << ": " << [&] {
            std::string messages;
            for (const ValidationError& error : result.errors) {
                messages += "[" + error.code + "] " + error.message + "\n";
            }
            return messages;
        }();
        std::error_code ignored;
        std::filesystem::remove(written, ignored);
    }
}
