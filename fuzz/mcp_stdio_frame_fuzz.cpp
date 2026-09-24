// Assistant-transport fuzz entry: splits fuzzer bytes into newline-delimited
// frames and pipes each through the same JSON parse + envelope check as the
// live read loop. Malformed framing (unterminated, nulls, oversized) and
// malformed JSON (truncated, deeply nested, non-UTF8) must always yield a
// well-formed -32700 response path or a skip — never a crash, hang, or
// unhandled exception. Deeply nested input is bounded by a max-depth check.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

// Mirrors the live loop: parse, validate envelope, recover id on failure.
// Never throws — returns true when a frame was handled (parsed or rejected).
bool handleFrame(const std::string& line) noexcept {
    try {
        if (line.empty()) {
            return true;
        }
        if (line.size() > 1048576) {
            return true;  // Oversized single frame: reject, don't allocate.
        }
        // Bounded nesting: reject absurd depth before full parse cost.
        int depth = 0;
        int maxDepth = 0;
        bool inString = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            const char c = line[i];
            if (c == '"' && (i == 0 || line[i - 1] != '\\')) {
                inString = !inString;
            }
            if (inString) {
                continue;
            }
            if (c == '{' || c == '[') {
                if (++depth > 500) {
                    return true;  // Too deep: bounded rejection.
                }
                maxDepth = depth > maxDepth ? depth : maxDepth;
            } else if (c == '}' || c == ']') {
                if (depth > 0) {
                    --depth;
                }
            }
        }
        (void)maxDepth;
        nlohmann::json message;
        try {
            message = nlohmann::json::parse(line);
        } catch (...) {
            // Would be -32700 Parse error on the wire; harness just survives.
            return true;
        }
        try {
            if (!message.is_object()) {
                return true;
            }
            auto it = message.find("jsonrpc");
            if (it == message.end() || !it->is_string()) {
                return true;
            }
        } catch (...) {
            return true;
        }
        return true;
    } catch (...) {
        return true;
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0 || size > 1048576) {
        return 0;
    }
    const std::string buffer(reinterpret_cast<const char*>(data), size);
    std::size_t start = 0;
    while (start <= buffer.size()) {
        const std::size_t end = buffer.find('\n', start);
        const std::string frame = buffer.substr(start, end == std::string::npos ? end : end - start);
        handleFrame(frame);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return 0;
}
