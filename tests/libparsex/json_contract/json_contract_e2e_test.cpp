// End-to-end sanitizer-validation test (PAR-177): a combined fixture set
// exercising wrapEnvelope() across both existing kinds (validationResult,
// diffReport) with realistic data, each validated against its schema via
// json-schema-validator. This test runs in every build; under
// ENABLE_SANITIZERS (ASan+UBSan via the build-asan preset, exercised by CI's
// Linux full and macOS leak-off legs) it is the zero-findings gate for the
// JSON contract helpers, matching the closing pattern of every prior Feature.
//
// macOS note (Apple Silicon — see PAR-18): run locally with
// ASAN_OPTIONS=detect_leaks=0; the Linux CI leg (PAR-23) is authoritative
// for "zero leaks".
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <parsex/diff/diff_report.hpp>
#include <parsex/json_contract/envelope.hpp>
#include <parsex/json_contract/schema_validate.hpp>
#include <parsex/validator/validation_result.hpp>

namespace {

namespace fs = std::filesystem;

fs::path schemasDir() {
#ifdef PARSEX_SCHEMAS_DIR
    return fs::path(PARSEX_SCHEMAS_DIR);
#else
    return fs::path("schemas");
#endif
}

// Realistic Validator output: an error with location plus a warning with
// type context, as validateCanSemantics()/validateReferences() produce.
ValidationResult realisticValidationResult() {
    ValidationResult result;
    result.errors.push_back({.severity = Severity::Error,
                             .code = "can.dlc_mismatch",
                             .message = "DLC 8 != 64",
                             .location = RawSpan{.startOffset = 128,
                                                 .endOffset = 256,
                                                 .lineNumber = 12},
                             .path = std::string("/Cluster/CAN/Frame_FNew")});
    result.errors.push_back({.severity = Severity::Warning,
                             .code = "ref.dest_unrecognized",
                             .message = "DEST company-X unrecognized",
                             .expectedType = std::string("FRAME"),
                             .actualType = std::string("PDU")});
    return result;
}

// Realistic Diff output: one entry per category plus an ambiguity
// diagnostic, mirroring the golden fixture's coverage.
DiffReport realisticDiffReport() {
    DiffReport report;
    DiffEntry added;
    added.kind = DiffKind::Added;
    added.elementType = "Frame";
    added.newPath = "/FNew";
    report.entries.push_back(added);
    DiffEntry removed;
    removed.kind = DiffKind::Removed;
    removed.elementType = "Signal";
    removed.oldPath = "/SOld";
    report.entries.push_back(removed);
    DiffEntry moved;
    moved.kind = DiffKind::Moved;
    moved.elementType = "Frame";
    moved.oldPath = "/FMoveOld";
    moved.newPath = "/FMoveNew";
    report.entries.push_back(moved);
    DiffEntry modified;
    modified.kind = DiffKind::Modified;
    modified.elementType = "Frame";
    modified.oldPath = "/F/Old";
    modified.newPath = "/F/Old";
    modified.fieldDiffs.push_back({.fieldName = "length", .oldValue = "8", .newValue = "64"});
    report.entries.push_back(std::move(modified));
    report.diagnostics.push_back({.message = "ambiguous secondary key group", .elementType = "Frame"});
    return report;
}

}  // namespace

TEST(JsonContractE2ETest, BothKindsValidateAgainstCommittedSchemas) {
    const fs::path envelopeSchema = schemasDir() / "envelope.schema.json";

    const nlohmann::json validationOut = realisticValidationResult().toJson();
    const nlohmann::json diffOut = realisticDiffReport().toJson();

    std::string error;
    EXPECT_TRUE(
        parsex::json_contract::validatesAgainstSchema(validationOut, envelopeSchema, &error))
        << error;
    EXPECT_TRUE(parsex::json_contract::validatesAgainstSchema(diffOut, envelopeSchema, &error))
        << error;

    // The shared helper and the migrated call sites stay consistent.
    EXPECT_EQ(validationOut["contractVersion"], diffOut["contractVersion"]);
    EXPECT_EQ(validationOut["toolVersion"], diffOut["toolVersion"]);
}

TEST(JsonContractE2ETest, ReadmeIndexLinksResolveToCommittedFiles) {
    const fs::path dir = schemasDir();
    const fs::path readme = dir / "README.md";
    ASSERT_TRUE(fs::exists(readme)) << readme;
    std::ifstream in(readme);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (const char* file : {"envelope.schema.json", "validation_result.schema.json",
                             "diff_report.schema.json", "CONVENTIONS.md", "VERSIONING.md"}) {
        EXPECT_NE(text.find(file), std::string::npos) << "README must index " << file;
        EXPECT_TRUE(fs::exists(dir / file)) << "indexed file missing: " << file;
    }
}
