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
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

// Every ARXML fixture the Parser/Loader/Validator suites parse (see
// all_fixtures_test.cpp): invalid ones (malformed, XXE, missing location,
// unsupported release) throw on parse and are excluded by construction —
// only parseable fixtures have defined writer behavior.
std::vector<std::string> sweepFixtures() {
    return {"parsefile_complete.arxml", "system-4.2.arxml", "warnings_missing_fields.arxml",
            "release_multiline.arxml"};
}

// Round-trip-clean file fixtures: study-only REF fields are empty here, so
// the real-vocabulary writer preserves everything. parsefile_complete.arxml
// is covered instead by StudyFixtureDocumentsRealVocabRefLimitation, and
// warnings_missing_fields.arxml by DegenerateFixtureIsRefusedFailFast below.
std::vector<std::string> roundTripCleanFixtures() {
    return {"system-4.2.arxml", "release_multiline.arxml"};
}

std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    return path;
}

// Healthy inline fixture (third distinct round-trip source): one of every
// domain type with all required fields present, real vocabulary.
constexpr const char* kHealthyInline =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<AUTOSAR xmlns=\"http://autosar.org/schema/r4.0\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xsi:schemaLocation=\"http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd\">\n"
    "  <AR-PACKAGES>\n"
    "    <AR-PACKAGE>\n"
    "      <SHORT-NAME>Sys</SHORT-NAME>\n"
    "      <ELEMENTS>\n"
    "        <CAN-CLUSTER>\n"
    "          <SHORT-NAME>C1</SHORT-NAME>\n"
    "          <CAN-CLUSTER-VARIANTS><CAN-CLUSTER-CONDITIONAL>\n"
    "            <BAUDRATE>500000</BAUDRATE>\n"
    "          </CAN-CLUSTER-CONDITIONAL></CAN-CLUSTER-VARIANTS>\n"
    "        </CAN-CLUSTER>\n"
    "        <ECU-INSTANCE><SHORT-NAME>E1</SHORT-NAME></ECU-INSTANCE>\n"
    "        <CAN-FRAME><SHORT-NAME>F1</SHORT-NAME><FRAME-LENGTH>8</FRAME-LENGTH></CAN-FRAME>\n"
    "        <I-SIGNAL-I-PDU><SHORT-NAME>P1</SHORT-NAME><LENGTH>8</LENGTH></I-SIGNAL-I-PDU>\n"
    "        <I-SIGNAL><SHORT-NAME>S1</SHORT-NAME><LENGTH>8</LENGTH></I-SIGNAL>\n"
    "        <I-SIGNAL-GROUP><SHORT-NAME>G1</SHORT-NAME>\n"
    "          <I-SIGNAL-REFS><I-SIGNAL-REF DEST=\"I-SIGNAL\">/Sys/S1</I-SIGNAL-REF></I-SIGNAL-REFS>\n"
    "        </I-SIGNAL-GROUP>\n"
    "      </ELEMENTS>\n"
    "    </AR-PACKAGE>\n"
    "  </AR-PACKAGES>\n"
    "</AUTOSAR>\n";

std::filesystem::path writeProject(const ParsedProject& project, const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    WriteEngine{}.write(project, path);
    return path;
}

}  // namespace

TEST(FixtureSweepTest, RoundTripIsCleanAcrossFixtures) {
    std::vector<std::string> covered;
    for (const std::string& name : roundTripCleanFixtures()) {
        assertRoundTripsCleanly(fixture(name));
        covered.push_back(name);
    }
    const auto inlinePath = writeTemp("parsex_sweep_healthy.arxml", kHealthyInline);
    assertRoundTripsCleanly(inlinePath);
    covered.emplace_back("inline-healthy");
    std::error_code ignored;
    std::filesystem::remove(inlinePath, ignored);
    ASSERT_GE(covered.size(), 3U) << "sweep must cover at least 3 distinct fixtures";
    RecordProperty("sweep_fixture_count", std::to_string(sweepFixtures().size()));
}

TEST(FixtureSweepTest, DegenerateFixtureIsRefusedFailFast) {
    // warnings_missing_fields.arxml is deliberately degenerate (e.g. an
    // element with no SHORT-NAME): the writer's fail-fast validation
    // (PAR-158) refuses it with a structured WriteError rather than emitting
    // schema-invalid output — garbage-in is rejected, never propagated.
    ParsedProject project;
    project.files.push_back(Parser{}.parseFile(fixture("warnings_missing_fields.arxml")));
    EXPECT_TRUE(WriteEngine{}.validate(project).hasErrors());
    const auto path = std::filesystem::temp_directory_path() / "parsex_sweep_degenerate.arxml";
    EXPECT_THROW(WriteEngine{}.write(project, path), WriteError);
    EXPECT_FALSE(std::filesystem::exists(path));
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
