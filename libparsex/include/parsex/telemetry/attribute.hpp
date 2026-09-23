#pragma once

#include <cstdint>
#include <string>
#include <variant>

// Foundational attribute types for the Telemetry Feature (PAR-184).
//
// Attributes attach key/value context to a Span (e.g. file path, byte count).

namespace parsex::telemetry {

using AttributeValue = std::variant<std::string, std::int64_t, double, bool>;

struct Attribute {
    std::string key;
    AttributeValue value;
};

}  // namespace parsex::telemetry
