#include <parsex/telemetry/scoped_span.hpp>

#include <chrono>
#include <cassert>
#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include <parsex/telemetry/span.hpp>
#include <parsex/telemetry/telemetry_config.hpp>
#include <parsex/telemetry/telemetry_context.hpp>

namespace parsex::telemetry {
namespace {

using Clock = std::chrono::steady_clock;

[[nodiscard]] std::uint64_t nanosSince(const TimePoint& point, const TimePoint& origin) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(point - origin).count());
}

// Thread-local stack of in-flight span ids for automatic parent tracking.
// Each thread has its own nesting stack so spans on different threads are
// never miscategorized as parent/child of each other.
std::vector<SpanId>& activeSpanStack() {
    thread_local std::vector<SpanId> stack;
    return stack;
}

}  // namespace

TimePoint processStartTimePoint() {
    static const TimePoint start = Clock::now();
    return start;
}

ScopedSpan::ScopedSpan(std::string name) : name_(std::move(name)) {
    if (!TelemetryConfig::isEnabled()) {
        return;
    }
    active_ = true;
    id_ = nextSpanId();
    auto& stack = activeSpanStack();
    if (!stack.empty()) {
        parentId_ = stack.back();
    }
    startPoint_ = Clock::now();
    uncaughtAtConstruct_ = std::uncaught_exceptions();
    stack.push_back(id_);
}

ScopedSpan::~ScopedSpan() {
    if (!active_) {
        return;
    }
    const TimePoint endPoint = Clock::now();
    const TimePoint origin = processStartTimePoint();

    // Default status (PAR-188): explicit setStatus wins; otherwise Ok on clean
    // exit, Error if an exception is unwinding through this scope (mirrors
    // OpenTelemetry's Unset-resolves-to-success guidance).
    SpanStatus finalStatus = status_;
    if (!statusExplicit_) {
        finalStatus = (std::uncaught_exceptions() > uncaughtAtConstruct_) ? SpanStatus::Error
                                                                          : SpanStatus::Ok;
    }
    Span finished;
    finished.name = name_;
    finished.id = id_;
    finished.parentId = parentId_;
    finished.startNanos = nanosSince(startPoint_, origin);
    finished.endNanos = nanosSince(endPoint, origin);
    finished.attributes = std::move(attributes_);
    finished.status = finalStatus;

    auto& stack = activeSpanStack();
    if (!stack.empty() && stack.back() == id_) {
        stack.pop_back();
    } else {
        // Out-of-order destruction: pop by id match so a mis-scoped span
        // cannot leak a stale parent. Asserts in debug builds to catch caller
        // bugs (e.g. a ScopedSpan stored past its natural scope) early.
        bool found = false;
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            if (*it == id_) {
                stack.erase(std::next(it).base());
                found = true;
                break;
            }
        }
        assert(found && "ScopedSpan destroyed out of order or twice");
        (void)found;
    }

    TelemetryContext::currentTrace().spans.push_back(std::move(finished));
}

void ScopedSpan::setAttribute(std::string key, AttributeValue value) {
    if (!active_) {
        return;
    }
    attributes_.push_back(Attribute{std::move(key), std::move(value)});
}

void ScopedSpan::setStatus(SpanStatus status) {
    if (!active_) {
        return;
    }
    status_ = status;
    statusExplicit_ = true;
}

}  // namespace parsex::telemetry
