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

fs::path mcpBinary() {
#ifdef PARSEX_MCP_BINARY
    return fs::path(PARSEX_MCP_BINARY);
#else
    return fs::path("parsex-mcp");
#endif
}

fs::path fixturesDir() {
#ifdef PARSEX_FIXTURES_DIR
    return fs::path(PARSEX_FIXTURES_DIR);
#else
    return fs::path("tests/fixtures");
#endif
}

std::string runSession(const std::string& inputLines) {
    // Write requests to a temp file, pipe into the binary, capture stdout.
    // Stderr (diagnostics) is discarded so only protocol lines are approved.
    fs::path tmp = fs::temp_directory_path() / "bridge_session_input.txt";
    {
        FILE* out = fopen(tmp.string().c_str(), "w");
        if (!out) {
            throw std::runtime_error("cannot write temp session file");
        }
        fwrite(inputLines.data(), 1, inputLines.size(), out);
        fclose(out);
    }
#ifdef _WIN32
    const std::string cmd = "\"" + mcpBinary().string() + "\" < \"" + tmp.string() + "\" 2>NUL";
#else
    const std::string cmd = "\"" + mcpBinary().string() + "\" < \"" + tmp.string() + "\" 2>/dev/null";
#endif
    std::array<char, 4096> buf{};
    std::string captured;
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen failed: " + cmd);
    }
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        captured += buf.data();
    }
    ::pclose(pipe);
    return captured;
}

std::string goldenWritePath() {
    return (fs::temp_directory_path() / "bridge-golden-write.arxml").string();
}

std::string normalizeTranscript(std::string s) {
    const std::string dir = fixturesDir().string();
    std::string::size_type pos = 0;
    while ((pos = s.find(dir, pos)) != std::string::npos) {
        s.replace(pos, dir.size(), "<fixtures>");
        pos += 12;
    }
    // Normalize temp output paths so golden files stay machine-independent.
    // Replace the exact temp write path (raw and JSON-escaped forms) with a
    // stable placeholder, preserving surrounding text.
    const std::string tempPath = goldenWritePath();
    const std::string placeholder = "<tmp>/golden.arxml";
    pos = 0;
    while ((pos = s.find(tempPath, pos)) != std::string::npos) {
        s.replace(pos, tempPath.size(), placeholder);
        pos += placeholder.size();
    }
    // JSON-escaped form on Windows (backslashes doubled): C:\\...\\file.
    std::string escapedTemp = tempPath;
    {
        std::string doubled;
        for (char ch : escapedTemp) {
            if (ch == '\\') {
                doubled += "\\\\";
            }
            doubled += ch;
        }
        escapedTemp = std::move(doubled);
    }
    if (escapedTemp != tempPath) {
        pos = 0;
        while ((pos = s.find(escapedTemp, pos)) != std::string::npos) {
            s.replace(pos, escapedTemp.size(), placeholder);
            pos += placeholder.size();
        }
    }
    // Legacy prefix form (Unix-only runs) for already-approved baselines.
    const std::string tmpPrefix = "/tmp/bridge-golden";
    pos = 0;
    while ((pos = s.find(tmpPrefix, pos)) != std::string::npos) {
        auto end = s.find(".arxml", pos);
        if (end == std::string::npos) {
            break;
        }
        s.replace(pos, end + 6 - pos, "<tmp>/golden.arxml");
        pos += 18;
    }
    // Pretty-print each line with stable ordering to avoid key-order churn.
    std::string out;
    std::string::size_type start = 0;
    while (start < s.size()) {
        auto end = s.find('\n', start);
        std::string line = (end == std::string::npos) ? s.substr(start) : s.substr(start, end - start);
        if (!line.empty()) {
            try {
                auto doc = nlohmann::json::parse(line);
                out += doc.dump(2) + "\n";
            } catch (...) {
                out += line + "\n";
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return out;
}

std::string basicSessionInput() {
    std::string fixture = (fixturesDir() / "schema_valid.arxml").string();
    std::string escaped;
    for (char ch : fixture) {
        if (ch == '\\' || ch == '"') {
            escaped += '\\';
        }
        escaped += ch;
    }
    return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\"}\n"
           "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}\n"
           "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"parse_arxml\","
           "\"arguments\":{\"path\":\"" +
           escaped + "\"}}}\n";
}

std::string jsonEscape(const std::string& value) {
    std::string out;
    for (char ch : value) {
        if (ch == '\\' || ch == '"') {
            out += '\\';
        }
        out += ch;
    }
    return out;
}

std::string validateSessionInput() {
    const std::string file = jsonEscape((fixturesDir() / "schema_valid.arxml").string());
    return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"validate_arxml\","
           "\"arguments\":{\"path\":\"" +
           file + "\"}}}\n"
           "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"validate_arxml\","
           "\"arguments\":{\"path\":\"" +
           file + "\",\"strict\":true}}}\n";
}

std::string diffSessionInput() {
    const std::string same = jsonEscape((fixturesDir() / "schema_valid.arxml").string());
    const std::string other = jsonEscape((fixturesDir() / "system-4.2.arxml").string());
    return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"diff_arxml\","
           "\"arguments\":{\"basePath\":\"" +
            same + "\",\"targetPath\":\"" + same + "\"}}}\n"
            "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"diff_arxml\","
            "\"arguments\":{\"basePath\":\"" +
            same + "\",\"targetPath\":\"" + other + "\"}}}\n";
}

std::string writeSessionInput() {
    const std::string file = jsonEscape((fixturesDir() / "schema_valid.arxml").string());
    const std::string outFile = jsonEscape(goldenWritePath());
    return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"write_arxml\","
           "\"arguments\":{\"path\":\"" +
           file + "\",\"outputPath\":\"" + outFile + "\"}}}\n"
           "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"write_arxml\","
           "\"arguments\":{\"path\":\"" +
           file + "\",\"outputPath\":\"" + outFile + "\",\"apply\":true}}}\n";
}

std::string errorSessionInput() {
    const std::string bad = jsonEscape((fixturesDir() / "malformed_unclosed.arxml").string());
    return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"parse_arxml\","
           "\"arguments\":{\"path\":\"" +
           bad + "\"}}}\n"
           "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"unknown_tool\","
           "\"arguments\":{}}}\n"
           "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"parse_arxml\","
           "\"arguments\":{}}}\n";
}

std::string versionSessionInput() {
    return "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"_meta\":{"
           "\"io.modelcontextprotocol/protocolVersion\":\"2099-01-01\"}}\n"
           "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"initialize\",\"params\":{"
           "\"protocolVersion\":\"2024-01-01\"}}\n";
}

}  // namespace

// Golden transcripts over real binary sessions. Reviewed baselines live in
// approval_test.*.approved.txt. To regenerate after an intentional change:
// run the test, review the .received.txt diff, then copy it over the
// .approved.txt when the new output is correct.

TEST(McpApproval, BasicSession) {
    std::string output = normalizeTranscript(runSession(basicSessionInput()));
    ApprovalTests::Approvals::verify(output);
}

TEST(McpApproval, ValidateSession) {
    if (schemasMissing()) {
        GTEST_SKIP() << "user-supplied 4.2.2 schema not present";
    }
    std::string output = normalizeTranscript(runSession(validateSessionInput()));
    ApprovalTests::Approvals::verify(output);
}

TEST(McpApproval, DiffSession) {
    std::string output = normalizeTranscript(runSession(diffSessionInput()));
    ApprovalTests::Approvals::verify(output);
}

TEST(McpApproval, WriteSession) {
    std::filesystem::remove(goldenWritePath());
    std::string output = normalizeTranscript(runSession(writeSessionInput()));
    std::filesystem::remove(goldenWritePath());
    ApprovalTests::Approvals::verify(output);
}

TEST(McpApproval, ErrorSession) {
    std::string output = normalizeTranscript(runSession(errorSessionInput()));
    ApprovalTests::Approvals::verify(output);
}

TEST(McpApproval, VersionSession) {
    std::string output = normalizeTranscript(runSession(versionSessionInput()));
    ApprovalTests::Approvals::verify(output);
}
