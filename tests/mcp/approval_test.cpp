#define APPROVALS_GOOGLETEST
#include "ApprovalTests.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

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
    std::string cmd = "\"" + mcpBinary().string() + "\" < \"" + tmp.string() + "\" 2>/dev/null";
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

std::string normalizeTranscript(std::string s) {
    std::string dir = fixturesDir().string();
    std::string::size_type pos = 0;
    while ((pos = s.find(dir, pos)) != std::string::npos) {
        s.replace(pos, dir.size(), "<fixtures>");
        pos += 12;
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

}  // namespace

// Golden transcripts over real binary sessions. Reviewed baselines live in
// approval_test.*.approved.txt. To regenerate after an intentional change:
// run the test, review the .received.txt diff, then copy it over the
// .approved.txt when the new output is correct.

TEST(McpApproval, BasicSession) {
    std::string output = normalizeTranscript(runSession(basicSessionInput()));
    ApprovalTests::Approvals::verify(output);
}
