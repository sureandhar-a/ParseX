#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Interim telemetry seam for SchemaRegistry, following the Telemetry
// component's explicit-pointer-parameter pattern (nullable raw pointer:
// timed when non-null, zero-cost when null). The real Telemetry component
// will replace or adopt this type; call sites already pass
// OperationTelemetry* either way, so they won't churn when it lands.
struct OperationTelemetry {
    struct Sample {
        std::string name;
        std::chrono::nanoseconds elapsed{0};
    };

    std::vector<Sample> samples;

    void record(std::string name, std::chrono::nanoseconds elapsed) {
        samples.push_back(Sample{std::move(name), elapsed});
    }
};

// Times the enclosing scope into telemetry under the given operation name.
// A null pointer costs nothing (one branch, no allocation, no record).
class ScopedTimer {
public:
    ScopedTimer(OperationTelemetry* telemetry, std::string_view name)
        : telemetry_(telemetry), name_(name), start_(std::chrono::steady_clock::now()) {}
    ~ScopedTimer() {
        if (telemetry_ != nullptr) {
            telemetry_->record(std::string(name_), std::chrono::steady_clock::now() - start_);
        }
    }
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    OperationTelemetry* telemetry_;
    std::string_view name_;
    std::chrono::steady_clock::time_point start_;
};
