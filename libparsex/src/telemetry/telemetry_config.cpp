#include <parsex/telemetry/telemetry_config.hpp>

#include <atomic>

namespace parsex::telemetry {

std::atomic<bool>& TelemetryConfig::flag() {
    static std::atomic<bool> enabled{false};
    return enabled;
}

bool TelemetryConfig::isEnabled() noexcept {
#ifdef PARSEX_TELEMETRY_DISABLED
    // Compiled-out build (PAR-191): telemetry can never be enabled.
    return false;
#else
    return flag().load(std::memory_order_relaxed);
#endif
}

void TelemetryConfig::setEnabled(bool enabled) noexcept {
#ifdef PARSEX_TELEMETRY_DISABLED
    // Documented no-op: the project has no logging facility yet, so silently
    // ignore (rather than failing the build) so existing setEnabled(true)
    // call sites still compile in telemetry-free builds.
    (void)enabled;
#else
    flag().store(enabled, std::memory_order_relaxed);
#endif
}

}  // namespace parsex::telemetry
