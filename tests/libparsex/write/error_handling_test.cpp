// Structured error-handling tests (PAR-158): write() fails clearly on
// incomplete input via the Validator's ValidationError convention — never a
// crash or a silently malformed file — and never leaves partial output.

#include <gtest/gtest.h>

#include <parsex/model/parsed_file.hpp>
#include <parsex/model/parsed_project.hpp>
#include <parsex/write/write_engine.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void writeText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    out.close();
}

}  // namespace

TEST(WriteErrorHandlingTest, MissingShortNameThrowsStructuredErrorWithoutOutput) {
    ParsedProject project;
    ParsedFile file;
    file.autosarRelease = "4.4.0";
    Frame frame;  // shortName left empty — schema-mandated, genuinely required.
    frame.length = 8U;
    file.frames.push_back(frame);
    project.files.push_back(file);

    const WriteEngine engine;
    const ValidationResult problems = engine.validate(project);
    ASSERT_TRUE(problems.hasErrors());
    EXPECT_EQ(problems.errors.front().code, "write.missing_field");
    EXPECT_NE(problems.errors.front().message.find("SHORT-NAME"), std::string::npos);
    EXPECT_NE(problems.errors.front().message.find("Frame[0]"), std::string::npos);

    const auto path = std::filesystem::temp_directory_path() / "parsex_write_err.arxml";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    try {
        engine.write(project, path);
        FAIL() << "expected WriteError";
    } catch (const WriteError& error) {
        EXPECT_TRUE(error.result.hasErrors());
    }
    EXPECT_FALSE(std::filesystem::exists(path)) << "failed write must produce no output file";
}

TEST(WriteErrorHandlingTest, FailedWriteLeavesExistingFileUntouched) {
    ParsedProject project;
    ParsedFile file;
    file.autosarRelease = "4.4.0";
    Pdu pdu;  // shortName left empty.
    file.pdus.push_back(pdu);
    project.files.push_back(file);

    const auto path = std::filesystem::temp_directory_path() / "parsex_write_kept.arxml";
    writeText(path, "sentinel-content");
    EXPECT_THROW(WriteEngine{}.write(project, path), WriteError);
    EXPECT_EQ(readFile(path), "sentinel-content");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(WriteErrorHandlingTest, ValidProjectSucceedsWithoutFalsePositives) {
    ParsedProject project;
    ParsedFile file;
    file.autosarRelease = "4.4.0";
    Frame frame;
    frame.common.shortName = "Frame_1";
    frame.length = 8U;
    file.frames.push_back(frame);
    project.files.push_back(file);

    EXPECT_FALSE(WriteEngine{}.validate(project).hasErrors());
    const auto path = std::filesystem::temp_directory_path() / "parsex_write_ok.arxml";
    EXPECT_NO_THROW(WriteEngine{}.write(project, path));
    EXPECT_NE(readFile(path).find("Frame_1"), std::string::npos);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
