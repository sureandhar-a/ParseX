#pragma once

#include <atomic>

// Runtime on/off switch for Telemetry (PAR-190 foundation, needed early so
// ScopedSpan can short-circuit when disabled).
//
// Off by default: most runs should pay no recording cost. Exposed via static
// accessor backed by an atomic so it is safe to read from multiple threads.

namespace parsex::telemetry {

class TelemetryConfig {
public:
    TelemetryConfig() = delete;

    [[nodiscard]] static bool isEnabled() noexcept;
    static void setEnabled(bool enabled) noexcept;

private:
    static std::atomic<bool>& flag();
};

}  // namespace parsex::telemetry
