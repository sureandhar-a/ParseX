#include <parsex/telemetry/span_status.hpp>

namespace parsex::telemetry {

const char* toString(SpanStatus status) noexcept {
    switch (status) {
        case SpanStatus::Unset:
            return "unset";
        case SpanStatus::Ok:
            return "ok";
        case SpanStatus::Error:
            return "error";
    }
    return "unset";
}

}  // namespace parsex::telemetry
