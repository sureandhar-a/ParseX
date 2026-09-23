#include <parsex/telemetry/telemetry_context.hpp>

namespace parsex::telemetry {

Trace& TelemetryContext::currentTrace() {
    static Trace trace;
    return trace;
}

nlohmann::json TelemetryContext::currentTraceAsJson() {
    return currentTrace().toJson();
}

void TelemetryContext::reset() {
    currentTrace().spans.clear();
}

}  // namespace parsex::telemetry
