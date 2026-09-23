#pragma once

#include <vector>

#include <nlohmann/json.hpp>

#include "parsex/telemetry/span.hpp"

// Trace data structure for the Telemetry Feature (PAR-185).
//
// A flat vector of Spans, mirroring OpenTelemetry's own wire representation
// (flat span list, each with its own parentId) so Trace::toJson() in PAR-182
// is a straight array mapping instead of a tree walk.

namespace parsex::telemetry {

struct Trace {
    std::vector<Span> spans;

    // Structured JSON rendering (PAR-195): envelope-wrapped per
    // schemas/envelope.schema.json with kind "telemetryReport". Root spans
    // omit parentSpanId (omit-over-null per CONVENTIONS.md).
    [[nodiscard]] nlohmann::json toJson() const;
};

[[nodiscard]] std::vector<const Span*> children(const Trace& trace, SpanId parent);

}  // namespace parsex::telemetry
