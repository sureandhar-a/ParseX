#include <parsex/telemetry/telemetry_config.hpp>

#include <atomic>

namespace parsex::telemetry {

std::atomic<bool>& TelemetryConfig::flag() {
    static std::atomic<bool> enabled{false};
    return enabled;
}

bool TelemetryConfig::isEnabled() noexcept {
    return flag().load(std::memory_order_relaxed);
}

void TelemetryConfig::setEnabled(bool enabled) noexcept {
    flag().store(enabled, std::memory_order_relaxed);
}

}  // namespace parsex::telemetry
