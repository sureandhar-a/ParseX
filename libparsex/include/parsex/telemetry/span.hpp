#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "parsex/telemetry/attribute.hpp"
#include "parsex/telemetry/span_id.hpp"
#include "parsex/telemetry/span_status.hpp"

// Span data structure for the Telemetry Feature (PAR-185).
//
// Per the OpenTelemetry Span model: name, id, optional parent, start/end
// timestamps (nanoseconds, populated by ScopedSpan in PAR-180), attributes,
// status. Flat storage — parent/child expressed via parentId.

namespace parsex::telemetry {

struct Span {
    std::string name;
    SpanId id{};
    std::optional<SpanId> parentId;
    std::uint64_t startNanos{};
    std::uint64_t endNanos{};
    std::vector<Attribute> attributes;
    SpanStatus status{SpanStatus::Unset};
};

}  // namespace parsex::telemetry
