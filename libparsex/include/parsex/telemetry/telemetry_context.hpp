#pragma once

#include "parsex/telemetry/trace.hpp"

// Single active Trace for the process (PAR-199 foundation, needed early so
// ScopedSpan has somewhere to register finished spans).
//
// ParseX is a single-process CLI tool with one collection scope per run.
// currentTrace() accumulates for the lifetime of the process; reset() clears
// it (primarily for tests wanting a fresh trace per case).

namespace parsex::telemetry {

class TelemetryContext {
public:
    TelemetryContext() = delete;

    static Trace& currentTrace();
    static void reset();
};

}  // namespace parsex::telemetry
