#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

namespace {

// Placeholder dispatcher for the first transport step.
// Parses are handed off here; full routing arrives with later pieces.
// Keeps transport free of protocol logic.
void dispatchStub(const nlohmann::json& /*message*/) {}

}  // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        // Empty lines carry no message; skip without responding.
        if (line.empty()) {
            continue;
        }
        try {
            nlohmann::json message = nlohmann::json::parse(line);
            dispatchStub(message);
        } catch (const std::exception& ex) {
            // Minimal resilience for the first step: never let a bad line
            // kill the loop. Detailed error codes arrive next.
            std::cerr << "skipping malformed line: " << ex.what() << "\n";
        }
    }
    // EOF is the normal shutdown path for a stdio server.
    return 0;
}
