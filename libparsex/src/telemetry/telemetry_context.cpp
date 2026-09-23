#include <parsex/telemetry/telemetry_context.hpp>

namespace parsex::telemetry {

Trace& TelemetryContext::currentTrace() {
    static Trace trace;
    return trace;
}

void TelemetryContext::reset() {
    currentTrace().spans.clear();
}

}  // namespace parsex::telemetry
