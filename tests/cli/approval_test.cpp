#define APPROVALS_GOOGLETEST
#include "ApprovalTests.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace {

fs::path cliBinary() {
#ifdef PARSEX_CLI_BINARY
    return fs::path(PARSEX_CLI_BINARY);
#else
    return fs::path("parsex-cli");
#endif
}

fs::path fixturesDir() {
#ifdef PARSEX_FIXTURES_DIR
    return fs::path(PARSEX_FIXTURES_DIR);
#else
    return fs::path("tests/fixtures");
#endif
}

// Capture stdout of command (stderr redirected to /dev/null to hide [debug] line).
std::string captureStdout(const std::string& cmd) {
    std::array<char, 4096> buf{};
    std::string out;
    // Redirect stderr to /dev/null so only human stdout is approved.
    std::string fullCmd = cmd + " 2>/dev/null";
    FILE* pipe = ::popen(fullCmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen failed: " + fullCmd);
    }
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        out += buf.data();
    }
    int rc = ::pclose(pipe);
    // Approval tests should still pass even if command succeeds; we just verify output.
    // Hard fail if command failed (non-zero) to avoid approving error output.
    if (rc != 0) {
        throw std::runtime_error("command failed with code " + std::to_string(rc) + ": " + cmd + "\noutput: " + out);
    }
    return out;
}

std::string normalizeForApproval(std::string s) {
    // Make output machine-independent: replace absolute fixture dir with empty.
    std::string dir = fixturesDir().string();
    std::string::size_type pos = 0;
    while ((pos = s.find(dir, pos)) != std::string::npos) {
        s.replace(pos, dir.size(), "");
        // Remove leading "/" left after stripping dir (e.g. "/schema_valid" -> "schema_valid")
        if (pos < s.size() && s[pos] == '/') {
            s.erase(pos, 1);
        }
        pos += 1;
    }
    // Also normalize any absolute /tmp path prefix that appears in write output
    // (e.g. "/tmp/parsex_approval_write.arxml" is already stable, keep as is)
    return s;
}

std::string runParseHuman() {
    return captureStdout("\"" + cliBinary().string() + "\" parse --input \"" + (fixturesDir() / "schema_valid.arxml").string() + "\"");
}

std::string runValidateHuman() {
    return captureStdout("\"" + cliBinary().string() + "\" validate --input \"" + (fixturesDir() / "schema_valid.arxml").string() + "\"");
}

std::string runDiffHuman() {
    return captureStdout("\"" + cliBinary().string() + "\" diff --base \"" + (fixturesDir() / "schema_valid.arxml").string() + "\" --target \"" + (fixturesDir() / "schema_valid.arxml").string() + "\"");
}

std::string runWriteHuman() {
    return captureStdout("\"" + cliBinary().string() + "\" write --input \"" + (fixturesDir() / "schema_valid.arxml").string() + "\" --output /tmp/parsex_approval_write.arxml");
}

}  // namespace

// PAR-228: golden-file (approval) tests over real subcommand invocations.
// Each test verifies human-readable stdout; .approved.txt files are reviewed and committed.
// To regenerate after intentional output change: review the .received.txt diff, then copy it over the .approved.txt.

TEST(ApprovalTests, ParseHuman) {
    auto output = normalizeForApproval(runParseHuman());
    ApprovalTests::Approvals::verify(output);
}

TEST(ApprovalTests, ValidateHuman) {
    auto output = normalizeForApproval(runValidateHuman());
    ApprovalTests::Approvals::verify(output);
}

TEST(ApprovalTests, DiffHuman) {
    auto output = normalizeForApproval(runDiffHuman());
    ApprovalTests::Approvals::verify(output);
}

TEST(ApprovalTests, WriteHuman) {
    auto output = normalizeForApproval(runWriteHuman());
    ApprovalTests::Approvals::verify(output);
}
