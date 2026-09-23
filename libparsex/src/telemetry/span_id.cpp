#include <parsex/telemetry/span_id.hpp>

#include <atomic>
#include <cstdint>

namespace parsex::telemetry {

SpanId nextSpanId() {
    static std::atomic<SpanId> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace parsex::telemetry
