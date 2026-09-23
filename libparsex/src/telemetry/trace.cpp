#include <parsex/telemetry/trace.hpp>

namespace parsex::telemetry {

std::vector<const Span*> children(const Trace& trace, SpanId parent) {
    std::vector<const Span*> result;
    for (const auto& span : trace.spans) {
        if (span.parentId.has_value() && span.parentId.value() == parent) {
            result.push_back(&span);
        }
    }
    return result;
}

}  // namespace parsex::telemetry
