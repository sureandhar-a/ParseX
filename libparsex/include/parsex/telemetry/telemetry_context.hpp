#pragma once

#include "parsex/telemetry/trace.hpp"

// Public facade for retrieving a run's telemetry (PAR-199).
//
// ParseX is a single-process CLI tool with one collection scope per run.
// currentTrace() accumulates for the lifetime of the process; reset() clears
// it (primarily for tests wanting a fresh trace per case). Deliberately no
// automatic clearing and no reset-free fresh-trace API: single command per
// invocation is the current usage, so a documented non-goal for now.

namespace parsex::telemetry {

class TelemetryContext {
public:
    TelemetryContext() = delete;

    static Trace& currentTrace();
    static nlohmann::json currentTraceAsJson();
    static void reset();
};

}  // namespace parsex::telemetry
