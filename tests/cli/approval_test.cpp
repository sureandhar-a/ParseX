#define APPROVALS_GOOGLETEST
#include "ApprovalTests.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <parsex/schema/schema_registry.hpp>
#include <parsex/schema/schema_resolution_error.hpp>

namespace fs = std::filesystem;

namespace {

bool schemasMissing() {
    try {
        (void)resolveSchema("4.2.2");
        return false;
    } catch (const SchemaResolutionError& err) {
        if (err.reason() != SchemaResolutionReason::UnsupportedRelease) {
            throw;
        }
        return true;
    } catch (...) {
        return true;
    }
}

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

// Capture stdout of command (stderr discarded to hide [debug] line).
std::string captureStdout(const std::string& cmd) {
    std::array<char, 4096> buf{};
    std::string out;
    // Redirect stderr to the null device so only stdout is approved.
#ifdef _WIN32
    const std::string fullCmd = cmd + " 2>NUL";
#else
    const std::string fullCmd = cmd + " 2>/dev/null";
#endif
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
    const std::string dir = fixturesDir().string();
    std::string::size_type pos = 0;
    while ((pos = s.find(dir, pos)) != std::string::npos) {
        s.replace(pos, dir.size(), "");
        // Remove leading "/" left after stripping dir (e.g. "/schema_valid" -> "schema_valid")
        if (pos < s.size() && s[pos] == '/') {
            s.erase(pos, 1);
        }
        pos += 1;
    }
    // Normalize the temp write path to the stable /tmp placeholder used in
    // approved baselines (temp_directory_path() varies per machine/OS).
    const std::string writeName = "parsex_approval_write.arxml";
    const std::string writePlaceholder = "/tmp/parsex_approval_write.arxml";
    pos = 0;
    while ((pos = s.find(writeName, pos)) != std::string::npos) {
        // Skip occurrences already inside the placeholder (avoid self-match loop).
        if (pos >= 5 && s.compare(pos - 5, 5, "/tmp/") == 0) {
            pos += writeName.size();
            continue;
        }
        // Walk back to the start of the path (space, quote, or start).
        std::string::size_type start = s.rfind(' ', pos);
        const std::string::size_type quote = s.rfind('"', pos);
        if (quote != std::string::npos && (start == std::string::npos || quote > start)) {
            start = quote;
        }
        if (start == std::string::npos) {
            start = 0;
        } else {
            start += 1;
        }
        s.replace(start, pos + writeName.size() - start, writePlaceholder);
        pos = start + writePlaceholder.size();
    }
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

std::string approvalWritePath() {
    return (fs::temp_directory_path() / "parsex_approval_write.arxml").string();
}

std::string runWriteHuman() {
    return captureStdout("\"" + cliBinary().string() + "\" write --input \"" +
                         (fixturesDir() / "schema_valid.arxml").string() + "\" --output \"" +
                         approvalWritePath() + "\"");
}

std::string runParseJson() {
    return captureStdout("\"" + cliBinary().string() + "\" --json parse --input \"" + (fixturesDir() / "schema_valid.arxml").string() + "\"");
}

std::string runValidateJson() {
    return captureStdout("\"" + cliBinary().string() + "\" --json validate --input \"" + (fixturesDir() / "schema_valid.arxml").string() + "\"");
}

std::string runDiffJson() {
    return captureStdout("\"" + cliBinary().string() + "\" --json diff --base \"" + (fixturesDir() / "schema_valid.arxml").string() + "\" --target \"" + (fixturesDir() / "schema_valid.arxml").string() + "\"");
}

std::string runWriteJson() {
    return captureStdout("\"" + cliBinary().string() + "\" --json write --input \"" +
                         (fixturesDir() / "schema_valid.arxml").string() + "\" --output \"" +
                         approvalWritePath() + "\"");
}

std::string normalizeJsonForApproval(std::string s) {
    // First apply path normalization (fixture dir).
    s = normalizeForApproval(std::move(s));
    // Then use JSON-aware normalization: parse and re-dump with stable 2-space indent.
    // This avoids false failures from key-ordering differences (e.g. nlohmann insertion order).
    // If ApprovalTests' JSON comparator is available, it would do similar; we do manual.
    try {
        auto j = nlohmann::json::parse(s);
        // Re-dump with sorted keys? nlohmann doesn't sort, but re-parsing then dumping
        // preserves the file's key order which is deterministic from wrapEnvelope.
        // To be extra stable, we output with dump(2) which is consistent.
        return j.dump(2) + "\n";
    } catch (...) {
        return s;
    }
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
    if (schemasMissing()) {
        GTEST_SKIP() << "user-supplied 4.2.2 schema not present";
    }
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

// PAR-229: golden-file coverage for --json output alongside human output.
// Both modes are tested for all four subcommands; a deliberate regression
// (temporarily break one field) is caught by the approval diff before revert.
// JSON-wise we use nlohmann::json parse+dumps to avoid key-ordering false failures.

TEST(ApprovalTests, ParseJson) {
    auto output = normalizeJsonForApproval(runParseJson());
    ApprovalTests::Approvals::verify(output);
}

TEST(ApprovalTests, ValidateJson) {
    if (schemasMissing()) {
        GTEST_SKIP() << "user-supplied 4.2.2 schema not present";
    }
    auto output = normalizeJsonForApproval(runValidateJson());
    ApprovalTests::Approvals::verify(output);
}

TEST(ApprovalTests, DiffJson) {
    auto output = normalizeJsonForApproval(runDiffJson());
    ApprovalTests::Approvals::verify(output);
}

TEST(ApprovalTests, WriteJson) {
    auto output = normalizeJsonForApproval(runWriteJson());
    ApprovalTests::Approvals::verify(output);
}
