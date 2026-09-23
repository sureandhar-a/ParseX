#pragma once

#include <parsex/telemetry/scoped_span.hpp>

// Ergonomic instrumentation macro for the Telemetry Feature (PAR-190,
// compile-time escape hatch PAR-191).
//
// With telemetry enabled (default): expands to a uniquely-named ScopedSpan;
// when runtime-disabled its cost is one isEnabled() branch (no clock calls,
// allocations, or stack pushes).
//
// With -DPARSEX_ENABLE_TELEMETRY=OFF (PARSEX_TELEMETRY_DISABLED defined):
// expands to ((void)0) — the compiler generates nothing, not even the branch.

#ifdef PARSEX_TELEMETRY_DISABLED
#define PARSEX_SPAN(name) ((void)0)
#else
#define PARSEX_SPAN_CONCAT_INNER(a, b) a##b
#define PARSEX_SPAN_CONCAT(a, b) PARSEX_SPAN_CONCAT_INNER(a, b)
#define PARSEX_SPAN(name) \
    ::parsex::telemetry::ScopedSpan PARSEX_SPAN_CONCAT(_parsex_span_, __LINE__)(name)
#endif
