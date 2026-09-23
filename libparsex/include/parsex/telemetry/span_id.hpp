#pragma once

#include <atomic>
#include <cstdint>

// Span identifier for the Telemetry Feature (PAR-185).
//
// Explicit ParseX simplification: monotonically-increasing counter, not a
// UUID or OpenTelemetry random ID. Only uniqueness within one local run is
// needed, never global uniqueness across machines or processes.

namespace parsex::telemetry {

using SpanId = std::uint64_t;

[[nodiscard]] SpanId nextSpanId();

}  // namespace parsex::telemetry
