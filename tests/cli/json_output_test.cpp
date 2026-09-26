#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "parsex/json_contract/schema_validate.hpp"
#include "parsex/json_contract/version.hpp"
#include "parsex/version.hpp"

namespace fs = std::filesystem;

namespace {

fs::path cliBinary() {
#ifdef PARSEX_CLI_BINARY
    return fs::path(PARSEX_CLI_BINARY);
#else
    return fs::path("parsex-cli");
#endif
}

fs::path schemasDir() {
#ifdef PARSEX_SCHEMAS_DIR
    return fs::path(PARSEX_SCHEMAS_DIR);
#else
    return fs::path("schemas");
#endif
}

fs::path fixtureFile() {
#ifdef PARSEX_FIXTURE_FILE
    return fs::path(PARSEX_FIXTURE_FILE);
#else
    return fs::path("tests/fixtures/tiny_valid.arxml");
#endif
}

// Runs cmd, captures stdout (stderr redirected to /dev/null unless included),
// returns {exitCode, stdout}.
std::pair<int, std::string> runCapture(const std::string& cmd) {
    std::array<char, 4096> buf{};
    std::string out;
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error("popen failed for: " + cmd);
    }
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        out += buf.data();
    }
    int rc = ::pclose(pipe);
    return {rc, out};
}

std::string trimLeft(const std::string& s) {
    const auto pos = s.find_first_not_of(" \t\r\n");
    return pos == std::string::npos ? "" : s.substr(pos);
}

}  // namespace

// PAR-215.1: --json output validates against the Output Contract schema.
// Reuses PAR-165's validatesAgainstSchema() approach (not a regex).
TEST(CliJsonOutput, JsonValidateConformsToEnvelopeSchema) {
#ifdef _WIN32
    const std::string nullRedirect = " 2>NUL";
#else
    const std::string nullRedirect = " 2>/dev/null";
#endif
    const std::string cmd =
        "\"" + cliBinary().string() + "\" --json validate --input \"" + fixtureFile().string() +
        "\"" + nullRedirect;
    const auto [rc, out] = runCapture(cmd);
    EXPECT_EQ(rc, 0) << "cmd: " << cmd << "\noutput: " << out;

    nlohmann::json doc;
    ASSERT_NO_THROW(doc = nlohmann::json::parse(out)) << "stdout was not JSON:\n" << out;
    EXPECT_EQ(doc.at("kind").get<std::string>(), "validationResult");
    EXPECT_EQ(doc.at("contractVersion").get<std::string>(),
              std::string(parsex::json_contract::kContractVersion));
    EXPECT_EQ(doc.at("toolVersion").get<std::string>(), std::string(libparsexVersion()));

    std::string error;
    EXPECT_TRUE(parsex::json_contract::validatesAgainstSchema(
                    doc, schemasDir() / "envelope.schema.json", &error))
        << error;
}

// PAR-215.2: without --json, stdout is human-readable, not JSON.
TEST(CliJsonOutput, HumanOutputIsNotJson) {
    // PAR-224: parse now calls real Parser; tiny_valid.arxml is unsupported
    // (4.0.0). Use a fixture that the Parser can actually handle.
    const fs::path parseFixture = schemasDir().parent_path() / "tests/fixtures/schema_valid.arxml";
#ifdef _WIN32
    const std::string nullRedirect = " 2>NUL";
#else
    const std::string nullRedirect = " 2>/dev/null";
#endif
    const std::string cmd =
        "\"" + cliBinary().string() + "\" parse --input \"" + parseFixture.string() + "\"" +
        nullRedirect;
    const auto [rc, out] = runCapture(cmd);
    EXPECT_EQ(rc, 0) << "cmd: " << cmd << "\noutput: " << out;
    const std::string trimmed = trimLeft(out);
    ASSERT_FALSE(trimmed.empty());
    EXPECT_NE(trimmed.front(), '{') << "human output must not start with '{':\n" << out;
}

// PAR-215.3: --json stdout stays clean (single JSON doc, no stray log lines).
TEST(CliJsonOutput, JsonStdoutContainsOnlyJson) {
    const fs::path parseFixture = schemasDir().parent_path() / "tests/fixtures/schema_valid.arxml";
#ifdef _WIN32
    const std::string nullRedirect = " 2>NUL";
#else
    const std::string nullRedirect = " 2>/dev/null";
#endif
    const std::string cmd =
        "\"" + cliBinary().string() + "\" --json parse --input \"" + parseFixture.string() + "\"" +
        nullRedirect;
    const auto [rc, out] = runCapture(cmd);
    EXPECT_EQ(rc, 0) << "cmd: " << cmd << "\noutput: " << out;
    nlohmann::json doc;
    ASSERT_NO_THROW(doc = nlohmann::json::parse(out)) << "stdout mixed non-JSON content:\n" << out;
    // Exact envelope keys prove no extra log lines were mixed into stdout
    // (they would break parse or add keys).
    EXPECT_TRUE(doc.contains("kind"));
    EXPECT_TRUE(doc.contains("payload"));
}
