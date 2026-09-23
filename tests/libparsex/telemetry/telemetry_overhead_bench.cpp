// Overhead benchmark for disabled telemetry (PAR-192).
//
// Standalone main() with manual steady_clock timing (the project has no
// benchmark harness yet — do not add a dependency just for this). Compares:
//   baseline (no span), runtime-disabled (isEnabled()==false, default),
//   enabled (isEnabled()==true, contrast only).
// Compile-time-disabled (PARSEX_TELEMETRY_DISABLED) is covered by the
// -DPARSEX_ENABLE_TELEMETRY=OFF build check in PAR-191: the macro expands to
// ((void)0), trivially identical to baseline.
#include <chrono>
#include <cstdint>
#include <cstdio>

#include <parsex/telemetry/scoped_span.hpp>
#include <parsex/telemetry/scoped_span_macro.hpp>
#include <parsex/telemetry/telemetry_config.hpp>
#include <parsex/telemetry/telemetry_context.hpp>

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kIterations = 2000000;

template <typename Body>
std::uint64_t timeLoop(Body&& body) {
    volatile std::uint64_t sink = 0;
    const auto start = Clock::now();
    for (int i = 0; i < kIterations; ++i) {
        body(sink, i);
    }
    const auto end = Clock::now();
    // Prevent the loop from being optimized away entirely.
    if (sink == 0xFFFFFFFFFFFFFFFFULL) {
        std::printf("sink=%llu\n", static_cast<unsigned long long>(sink));
    }
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

}  // namespace

int main() {
    using parsex::telemetry::TelemetryConfig;
    using parsex::telemetry::TelemetryContext;

    const std::uint64_t baseline = timeLoop([](volatile std::uint64_t& sink, int i) {
        sink += static_cast<std::uint64_t>(i);
    });

    TelemetryConfig::setEnabled(false);
    TelemetryContext::reset();
    const std::uint64_t runtimeDisabled = timeLoop([](volatile std::uint64_t& sink, int i) {
        PARSEX_SPAN("bench.iteration");
        sink += static_cast<std::uint64_t>(i);
    });
    const bool disabledEmpty = TelemetryContext::currentTrace().spans.empty();

    TelemetryConfig::setEnabled(true);
    TelemetryContext::reset();
    const std::uint64_t enabled = timeLoop([](volatile std::uint64_t& sink, int i) {
        PARSEX_SPAN("bench.iteration");
        sink += static_cast<std::uint64_t>(i);
    });
    const std::size_t enabledCount = TelemetryContext::currentTrace().spans.size();
    TelemetryConfig::setEnabled(false);
    TelemetryContext::reset();

    const auto perIter = [](std::uint64_t total) -> double {
        return static_cast<double>(total) / static_cast<double>(kIterations);
    };
    std::printf("iterations=%d\n", kIterations);
    std::printf("baseline: total=%lluns perIter=%.2fns\n", static_cast<unsigned long long>(baseline),
                perIter(baseline));
    std::printf("runtime-disabled: total=%lluns perIter=%.2fns overhead=%.2fns empty=%s\n",
                static_cast<unsigned long long>(runtimeDisabled), perIter(runtimeDisabled),
                perIter(runtimeDisabled) - perIter(baseline), disabledEmpty ? "true" : "false");
    std::printf("enabled: total=%lluns perIter=%.2fns count=%zu\n",
                static_cast<unsigned long long>(enabled), perIter(enabled), enabledCount);

    if (!disabledEmpty) {
        std::printf("FAIL: disabled run produced spans\n");
        return 1;
    }
    if (enabledCount != static_cast<std::size_t>(kIterations)) {
        std::printf("FAIL: enabled run produced %zu spans, expected %d\n", enabledCount,
                    kIterations);
        return 1;
    }
    return 0;
}
