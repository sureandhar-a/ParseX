// End-to-end sanitizer-validation test (PAR-159): a combined realistic
// fixture through the full public write() facade — write -> reparse ->
// diff-empty -> schema-valid — plus the error-handling path. This test runs
// in every build; under ENABLE_SANITIZERS (ASan+UBSan via the build-asan
// preset, exercised by CI's Linux full and macOS leak-off legs) it is the
// zero-findings gate for tree construction, sorting, and file writing.
//
// macOS note (Apple Silicon — see PAR-18): run locally with
// ASAN_OPTIONS=detect_leaks=0; the Linux CI leg (PAR-23) is authoritative
// for "zero leaks".

#include <gtest/gtest.h>

#include <parsex/diff/diff_engine.hpp>
#include <parsex/model/parsed_file.hpp>
#include <parsex/model/parsed_project.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/schema/schema_registry.hpp>
#include <parsex/schema/schema_resolution_error.hpp>
#include <parsex/validator/validator.hpp>
#include <parsex/write/write_engine.hpp>

#include <filesystem>
#include <string>

namespace {

// Combined fixture: all six domain types with realistic value ranges,
// including edge cases — an empty top-level collection (no EcuInstances in
// file[0], no PDU mappings on Frame_Empty), a SignalGroup, and extreme values
// (CAN FD length 64, 0x7 hex-style init value, signed signal).
//
// NOTE (pivot consequence): no value table is included — real I-SIGNALs have
// no schema-valid home for VALUE-TABLE (values live in COMPU-METHODs, out of
// v1 scope), so a multi-entry table cannot round-trip. That loss is
// documented in README.md's Known limitations and covered by
// StudyFixtureDocumentsRealVocabRefLimitation-style tests; this e2e asserts
// the clean path.
ParsedProject combinedProject() {
    ParsedProject project;

    ParsedFile populated;
    populated.autosarRelease = "4.4.0";
    populated.sourcePath = "combined-populated.arxml";

    Cluster cluster;
    cluster.common.shortName = "CAN_Cluster";
    cluster.common.category = "CAN";
    cluster.baudrate = 500000U;
    cluster.physicalChannels = {"can0", "can1"};
    populated.clusters.push_back(cluster);

    EcuInstance ecu;
    ecu.common.shortName = "ECU_A";
    ecu.controllers = {"CanCtrl_1"};
    populated.ecuInstances.push_back(ecu);

    Frame frame;
    frame.common.shortName = "Frame_FD";
    frame.length = 64U;
    frame.pdus = {{.pduShortNameRef = "Pdu_1", .startPosition = 0}};
    populated.frames.push_back(frame);

    Frame emptyFrame;
    emptyFrame.common.shortName = "Frame_Empty";
    emptyFrame.length = 8U;
    populated.frames.push_back(emptyFrame);

    Pdu pdu;
    pdu.common.shortName = "Pdu_1";
    pdu.length = 64U;
    // NOTE: mapping byteOrder is MSBFirst because PACKING-BYTE-ORDER is
    // omitted on write (both the real LAST-variant and absence map to the
    // domain default); a non-default order would not round-trip (documented
    // v1 scope limit, cf. write_elements.cpp).
    pdu.signalMappings = {{.signalShortNameRef = "Signal_Signed", .startPosition = 0}};
    populated.pdus.push_back(pdu);

    Signal signal;
    signal.common.shortName = "Signal_Signed";
    signal.startBit = 0U;
    signal.bitLength = 12U;
    signal.byteOrder = ByteOrder::MostSignificantByteFirst;
    signal.isSigned = true;
    signal.initValue = 7.0;
    populated.signals.push_back(signal);

    SignalGroup group;
    group.common.shortName = "Group_1";
    group.members = {"Signal_Signed"};
    populated.signalGroups.push_back(group);

    project.files.push_back(populated);

    // Second file: clusters but zero EcuInstances (empty top-level
    // collection edge case).
    ParsedFile sparse;
    sparse.autosarRelease = "4.4.0";
    sparse.sourcePath = "combined-sparse.arxml";
    Cluster lone;
    lone.common.shortName = "Lone_Cluster";
    sparse.clusters.push_back(lone);
    project.files.push_back(sparse);

    return project;
}

}  // namespace

TEST(WriteEndToEndTest, CombinedFixturePassesCleanly) {
    const ParsedProject project = combinedProject();
    const auto path = std::filesystem::temp_directory_path() / "parsex_write_e2e.arxml";
    WriteEngine{}.write(project, path);

    ParsedProject reparsed;
    reparsed.files.push_back(Parser{}.parseFile(path));
    // Two files in, one file out (single-package "Sys" merge): compare per
    // merged content — rebuild the expected merged single-file project.
    ParsedProject expected;
    ParsedFile merged;
    merged.autosarRelease = "4.4.0";
    for (const ParsedFile& file : project.files) {
        merged.clusters.insert(merged.clusters.end(), file.clusters.begin(), file.clusters.end());
        merged.ecuInstances.insert(merged.ecuInstances.end(), file.ecuInstances.begin(),
                                   file.ecuInstances.end());
        merged.frames.insert(merged.frames.end(), file.frames.begin(), file.frames.end());
        merged.pdus.insert(merged.pdus.end(), file.pdus.begin(), file.pdus.end());
        merged.signals.insert(merged.signals.end(), file.signals.begin(), file.signals.end());
        merged.signalGroups.insert(merged.signalGroups.end(), file.signalGroups.begin(),
                                   file.signalGroups.end());
    }
    expected.files.push_back(merged);
    const DiffReport report = DiffEngine{}.diff(expected, reparsed);
    EXPECT_TRUE(report.empty()) << report.toText();

    ParsedFile writtenFile;
    writtenFile.sourcePath = path;
    try {
        const SchemaResolutionResult resolved = resolveSchema("4.4.0");
        const ValidationResult result =
            Validator{}.validateSchema(writtenFile, resolved.schema.schemaHandle.get());
        EXPECT_TRUE(result.errors.empty());
    } catch (const SchemaResolutionError& error) {
        GTEST_SKIP() << "user-supplied 4.4.0 schema not present: " << error.what();
    }

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(WriteEndToEndTest, ErrorPathIsClean) {
    // The early-exit error path (validate-before-build: no xmlDoc is ever
    // allocated) runs cleanly too — error paths hide use-after-free and
    // double-free bugs when partial trees leak on exit.
    ParsedProject project = combinedProject();
    project.files.front().frames.front().common.shortName.clear();
    const auto path = std::filesystem::temp_directory_path() / "parsex_write_e2e_err.arxml";
    EXPECT_THROW(WriteEngine{}.write(project, path), WriteError);
    EXPECT_FALSE(std::filesystem::exists(path));
}
