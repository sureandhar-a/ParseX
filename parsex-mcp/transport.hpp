#pragma once

#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

// Output half of the stdio transport.
//
// Single chokepoint for everything written to stdout so framing stays
// consistent: compact JSON, exactly one trailing newline, flushed.
//
// Only this helper may write to std::cout. Diagnostics and logs go to stderr.
inline void writeMessage(const nlohmann::json& message) {
    std::cout << message.dump() << "\n";
    std::cout.flush();
}
