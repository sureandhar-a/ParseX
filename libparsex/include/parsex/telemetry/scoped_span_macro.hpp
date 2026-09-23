#pragma once

#include <parsex/telemetry/scoped_span.hpp>

// Ergonomic instrumentation macro for the Telemetry Feature (PAR-190).
//
// PARSEX_SPAN(name) is safe to leave in release builds: when telemetry is
// disabled its cost is one TelemetryConfig::isEnabled() branch inside the
// ScopedSpan constructor (no clock calls, allocations, or stack pushes — see
// ScopedSpan's active_ short-circuit). PARSEX_TELEMETRY_DISABLED (PAR-191)
// removes even that branch for builds that cannot tolerate it.

#define PARSEX_SPAN_CONCAT_INNER(a, b) a##b
#define PARSEX_SPAN_CONCAT(a, b) PARSEX_SPAN_CONCAT_INNER(a, b)
#define PARSEX_SPAN(name) \
    ::parsex::telemetry::ScopedSpan PARSEX_SPAN_CONCAT(_parsex_span_, __LINE__)(name)
