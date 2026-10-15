// CLI `export` subcommand: argument validation, stdout/-o behavior, --json
// envelope shape and schema conformance. Structure only: exported DBC/CSV
// content is covered by the library's own tests.
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "parsex/json_contract/schema_validate.hpp"

namespace fs = std::filesystem;

namespace {

struct CliRun {
    int code = -1;
    std::string out;
    std::string err;
};

std::string readAll(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream raw;
    raw << in.rdbuf();
    return raw.str();
}

// Runs the CLI with args; stdout and stderr are captured separately.
CliRun runCli(const std::string& args) {
    const fs::path errFile = fs::temp_directory_path() / "parsex_export_cli_stderr.txt";
    const std::string cmd =
        "\"" + std::string(PARSEX_CLI_BINARY) + "\" " + args + " 2>\"" + errFile.string() + "\"";
    std::array<char, 4096> buf{};
    CliRun run;
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        return run;
    }
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        run.out += buf.data();
    }
    run.code = ::pclose(pipe);
    run.err = readAll(errFile);
    fs::remove(errFile);
    return run;
}

std::string fixture() {
    return (fs::path(PARSEX_FIXTURES_DIR) / "system-4.2.arxml").string();
}

std::string exportArgs(const std::string& format, const std::string& extra = "") {
    return "export --input \"" + fixture() + "\" --format " + format + extra;
}

}  // namespace

#ifdef _WIN32
#define SKIP_ON_WINDOWS() GTEST_SKIP() << "popen shell harness is POSIX-specific"
#else
#define SKIP_ON_WINDOWS() (void)0
#endif

TEST(CliExport, MissingFormatIsUsageError) {
    SKIP_ON_WINDOWS();
    const CliRun run = runCli("export --input \"" + fixture() + "\"");
    EXPECT_NE(run.code, 0);
    EXPECT_NE(run.err.find("--format"), std::string::npos) << run.err;
    EXPECT_TRUE(run.out.empty()) << run.out;
}

TEST(CliExport, InvalidFormatIsUsageError) {
    SKIP_ON_WINDOWS();
    const CliRun run = runCli(exportArgs("xml"));
    EXPECT_NE(run.code, 0);
    EXPECT_NE(run.err.find("--format"), std::string::npos) << run.err;
}

TEST(CliExport, MissingInputFileIsUsageError) {
    SKIP_ON_WINDOWS();
    const CliRun run = runCli("export --input /nonexistent/parsex-none.arxml --format dbc");
    EXPECT_NE(run.code, 0);
    EXPECT_NE(run.err.find("does not exist"), std::string::npos) << run.err;
}

TEST(CliExport, JsonEnvelopeWithoutOutputInlinesContentAndConforms) {
    SKIP_ON_WINDOWS();
    for (const char* format : {"dbc", "csv"}) {
        const CliRun run = runCli(std::string("--json ") + exportArgs(format));
        ASSERT_EQ(run.code, 0) << run.err;
        nlohmann::json doc;
        ASSERT_NO_THROW(doc = nlohmann::json::parse(run.out)) << run.out;
        EXPECT_EQ(doc.at("kind"), "exportResult");
        const auto& payload = doc.at("payload");
        EXPECT_EQ(payload.at("sourcePath"), fixture());
        EXPECT_EQ(payload.at("format"), format);
        EXPECT_TRUE(payload.at("output").is_null());
        EXPECT_TRUE(payload.at("content").is_string());
        EXPECT_TRUE(payload.at("counts").at("frames").is_number_unsigned());
        EXPECT_TRUE(payload.at("counts").at("signals").is_number_unsigned());
        EXPECT_TRUE(payload.at("warnings").is_array());
        std::string error;
        EXPECT_TRUE(parsex::json_contract::validatesAgainstSchema(
            doc, fs::path(PARSEX_SCHEMAS_DIR) / "envelope.schema.json", &error))
            << error;
    }
}

TEST(CliExport, JsonEnvelopeWithOutputOmitsContentAndWritesFile) {
    SKIP_ON_WINDOWS();
    const fs::path outFile = fs::temp_directory_path() / "parsex_export_cli_json_out.dbc";
    fs::remove(outFile);
    const CliRun run = runCli("--json " + exportArgs("dbc", " --output \"" + outFile.string() + "\""));
    ASSERT_EQ(run.code, 0) << run.err;
    nlohmann::json doc = nlohmann::json::parse(run.out);
    const auto& payload = doc.at("payload");
    EXPECT_EQ(payload.at("output"), outFile.string());
    EXPECT_FALSE(payload.contains("content"));
    EXPECT_TRUE(fs::exists(outFile));
    std::string error;
    EXPECT_TRUE(parsex::json_contract::validatesAgainstSchema(
        doc, fs::path(PARSEX_SCHEMAS_DIR) / "envelope.schema.json", &error))
        << error;
    fs::remove(outFile);
}

TEST(CliExport, HumanWithOutputWritesFileAndPrintsOneLineSummary) {
    SKIP_ON_WINDOWS();
    const fs::path outFile = fs::temp_directory_path() / "parsex_export_cli_human_out.csv";
    fs::remove(outFile);
    const CliRun run = runCli(exportArgs("csv", " -o \"" + outFile.string() + "\""));
    ASSERT_EQ(run.code, 0) << run.err;
    EXPECT_TRUE(fs::exists(outFile));
    EXPECT_EQ(run.out.rfind("Exported ", 0), 0U) << run.out;
    EXPECT_NE(run.out.find(" frames, "), std::string::npos) << run.out;
    EXPECT_NE(run.out.find(" signals to " + outFile.string() + " (csv)\n"), std::string::npos)
        << run.out;
    EXPECT_EQ(std::count(run.out.begin(), run.out.end(), '\n'), 1);
    fs::remove(outFile);
}

TEST(CliExport, HumanWithoutOutputPrintsOnlyExportedTextToStdout) {
    SKIP_ON_WINDOWS();
    const CliRun run = runCli(exportArgs("dbc"));
    ASSERT_EQ(run.code, 0) << run.err;
    EXPECT_EQ(run.out.find("Exported "), std::string::npos) << "no summary line on stdout";
    EXPECT_EQ(run.out.find("parsex: warning"), std::string::npos) << "warnings belong on stderr";
}

TEST(CliExport, WarningsGoToStderrWithPrefixInHumanMode) {
    SKIP_ON_WINDOWS();
    const CliRun run = runCli(exportArgs("csv"));
    ASSERT_EQ(run.code, 0) << run.err;
    std::istringstream lines(run.err);
    std::string line;
    while (std::getline(lines, line)) {
        // Every stderr line is either the debug cache-dir note or a warning.
        EXPECT_TRUE(line.rfind("[debug]", 0) == 0 || line.rfind("parsex: warning: ", 0) == 0)
            << line;
    }
}
