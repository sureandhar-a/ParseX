#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "parsex/telemetry/attribute.hpp"
#include "parsex/telemetry/span_id.hpp"
#include "parsex/telemetry/span_status.hpp"

// RAII timing mechanism for the Telemetry Feature (PAR-187).
//
// steady_clock only: monotonic and documented as "most suitable for measuring
// intervals". system_clock can jump (NTP/manual changes); high_resolution_clock
// is implementation-defined and may alias system_clock.
//
// start/end are stored as nanoseconds since a single process-local reference
// point (steady_clock has no defined epoch, so an absolute "nanos since epoch"
// needs a process-local zero).

namespace parsex::telemetry {

using TimePoint = std::chrono::steady_clock::time_point;

[[nodiscard]] TimePoint processStartTimePoint();

class ScopedSpan {
public:
    // Takes string_view so the disabled path costs no allocation: the view
    // is copied (two words) and the constructor returns early on !isEnabled()
    // before any string construction, clock read, or stack push.
    explicit ScopedSpan(std::string_view name);
    ~ScopedSpan();

    ScopedSpan(const ScopedSpan&) = delete;
    ScopedSpan& operator=(const ScopedSpan&) = delete;
    ScopedSpan(ScopedSpan&&) = delete;
    ScopedSpan& operator=(ScopedSpan&&) = delete;

    void setAttribute(std::string key, AttributeValue value);
    void setStatus(SpanStatus status);

    [[nodiscard]] SpanId id() const noexcept {
        return id_;
    }
    [[nodiscard]] bool active() const noexcept {
        return active_;
    }

private:
    std::string name_;
    SpanId id_{};
    std::optional<SpanId> parentId_;
    TimePoint startPoint_{};
    std::vector<Attribute> attributes_;
    SpanStatus status_{SpanStatus::Unset};
    bool statusExplicit_{false};
    int uncaughtAtConstruct_{0};
    bool active_{false};
};

}  // namespace parsex::telemetry
