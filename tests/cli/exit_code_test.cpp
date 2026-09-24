#include <filesystem>
#include <ios>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "../../parsex-cli/exit_code.hpp"
#include "parsex/parser/parse_error.hpp"
#include "parsex/parser/project_error.hpp"
#include "parsex/parser/release_error.hpp"
#include "parsex/schema/schema_resolution_error.hpp"
#include "parsex/write/write_engine.hpp"

using namespace parsex::cli;

TEST(Classify, CliParseErrorMapsToUsage) {
    CLI::ParseError err("parse failed", CLI::ExitCodes::ExtrasError);
    EXPECT_EQ(classify(err), ExitCode::Usage);
}

TEST(Classify, ParseErrorIoMapsToIoErr) {
    ParseError err(ParseErrorReason::Io, std::filesystem::path("/tmp/missing.arxml"), "no such file");
    EXPECT_EQ(classify(err), ExitCode::IoErr);
}

TEST(Classify, ParseErrorSyntaxMapsToDataErr) {
    ParseError err(ParseErrorReason::Syntax, std::filesystem::path("/tmp/bad.arxml"), "syntax error");
    EXPECT_EQ(classify(err), ExitCode::DataErr);
}

TEST(Classify, SchemaMissingMapsToIoErr) {
    SchemaResolutionError err("R22-11", SchemaResolutionReason::SchemaFileMissing, "missing");
    EXPECT_EQ(classify(err), ExitCode::IoErr);
}

TEST(Classify, SchemaCorruptMapsToDataErr) {
    SchemaResolutionError err("4.2.2", SchemaResolutionReason::SchemaFileCorrupt, "corrupt");
    EXPECT_EQ(classify(err), ExitCode::DataErr);
}

TEST(Classify, SchemaUnsupportedMapsToDataErr) {
    SchemaResolutionError err("9.9.9", SchemaResolutionReason::UnsupportedRelease, "unsupported");
    EXPECT_EQ(classify(err), ExitCode::DataErr);
}

TEST(Classify, UnsupportedReleaseMapsToDataErr) {
    UnsupportedReleaseError err("autosar_9_9_9.xsd");
    EXPECT_EQ(classify(err), ExitCode::DataErr);
}

TEST(Classify, DanglingRefMapsToDataErr) {
    DanglingFileReferenceError err(std::vector<std::string>{"Foo"}, std::vector<std::filesystem::path>{"/tmp"});
    EXPECT_EQ(classify(err), ExitCode::DataErr);
}

TEST(Classify, WriteErrorMapsToDataErr) {
    ValidationResult vr;
    vr.errors.push_back(ValidationError{Severity::Error, "write.fail", "missing short-name"});
    WriteError err(std::move(vr));
    EXPECT_EQ(classify(err), ExitCode::DataErr);
}

TEST(Classify, FilesystemErrorMapsToIoErr) {
    std::filesystem::filesystem_error err("fs fail", std::error_code{});
    EXPECT_EQ(classify(err), ExitCode::IoErr);
}

TEST(Classify, IosFailureMapsToIoErr) {
    std::ios_base::failure err("io fail");
    EXPECT_EQ(classify(err), ExitCode::IoErr);
}

TEST(Classify, UnknownMapsToSoftware) {
    std::runtime_error err("unknown bug");
    EXPECT_EQ(classify(err), ExitCode::Software);
}
