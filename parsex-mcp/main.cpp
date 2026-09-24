#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "protocol.hpp"
#include "server.hpp"
#include "transport.hpp"

// Convention: writeMessage() is the only function allowed to write to
// std::cout. All diagnostics go to stderr so stdout stays pure protocol.

namespace {

// Routes one validated message. Discovery is optional: every method works
// whether or not the client asked for identity first. Unknown methods get a
// well-formed not-found reply so pipe tests can prove framing.
void dispatchStub(const nlohmann::json& message) {
    try {
        if (!message.is_object() || !message.contains("id")) {
            return;
        }
        const nlohmann::json id = message["id"];
        std::string method;
        try {
            if (message.contains("method") && message["method"].is_string()) {
                method = message["method"].get<std::string>();
            }
        } catch (...) {
        }
        if (method == "server/discover") {
            nlohmann::json response;
            response["jsonrpc"] = "2.0";
            response["id"] = id;
            response["result"] = buildDiscoverResult();
            writeMessage(response);
            return;
        }
        writeMessage(makeMethodNotFound(id));
    } catch (...) {
    }
}

}  // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        // Empty lines carry no message; skip without responding.
        if (line.empty()) {
            continue;
        }
        nlohmann::json message;
        try {
            message = nlohmann::json::parse(line);
        } catch (const std::exception& ex) {
            // No valid JSON means no reliable id. Try to recover one for a
            // proper Parse Error; otherwise log to stderr and keep looping.
            try {
                auto recovered = tryRecoverId(line);
                if (recovered.has_value()) {
                    try {
                        writeMessage(makeParseError(*recovered));
                    } catch (...) {
                    }
                } else {
                    std::cerr << "skipping malformed line: " << ex.what() << "\n";
                }
            } catch (...) {
                std::cerr << "skipping malformed line\n";
            }
            continue;
        }
        try {
            if (!isValidEnvelope(message)) {
                try {
                    writeMessage(makeInvalidRequest(idOrNull(message)));
                } catch (...) {
                }
                continue;
            }
            dispatchStub(message);
        } catch (...) {
            // Validation and dispatch must never escape; the loop survives
            // any sequence of malformed lines.
            std::cerr << "skipping invalid message\n";
        }
    }
    // EOF is the normal shutdown path for a stdio server.
    return 0;
}
