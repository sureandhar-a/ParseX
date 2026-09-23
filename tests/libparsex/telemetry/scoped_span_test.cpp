// Unit tests for ScopedSpan timing and nesting (PAR-189).
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>

#include <parsex/telemetry/scoped_span.hpp>
#include <parsex/telemetry/span_status.hpp>
#include <parsex/telemetry/telemetry_config.hpp>
#include <parsex/telemetry/telemetry_context.hpp>

namespace {

using parsex::telemetry::ScopedSpan;
using parsex::telemetry::SpanStatus;
using parsex::telemetry::TelemetryConfig;
using parsex::telemetry::TelemetryContext;

class ScopedSpanTest : public ::testing::Test {
protected:
    void SetUp() override {
        TelemetryConfig::setEnabled(true);
        TelemetryContext::reset();
    }
    void TearDown() override {
        TelemetryConfig::setEnabled(false);
        TelemetryContext::reset();
    }
};

TEST_F(ScopedSpanTest, SleepIntervalTimingAccuracy) {
    {
        ScopedSpan span("timed");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_EQ(TelemetryContext::currentTrace().spans.size(), 1U);
    const auto& finished = TelemetryContext::currentTrace().spans.front();
    EXPECT_EQ(finished.name, "timed");
    const std::uint64_t elapsed = finished.endNanos - finished.startNanos;
    EXPECT_GE(elapsed, 50ULL * 1000ULL * 1000ULL);
    EXPECT_LT(elapsed, 500ULL * 1000ULL * 1000ULL);
}

TEST_F(ScopedSpanTest, NestedSpansRecordParentChildAndStackUnwinds) {
    {
        ScopedSpan outer("outer");
        {
            ScopedSpan inner("inner");
        }
    }
    ASSERT_EQ(TelemetryContext::currentTrace().spans.size(), 2U);
    // Destructor appends inner first, then outer.
    const auto& inner = TelemetryContext::currentTrace().spans[0];
    const auto& outer = TelemetryContext::currentTrace().spans[1];
    EXPECT_EQ(inner.name, "inner");
    EXPECT_EQ(outer.name, "outer");
    ASSERT_TRUE(inner.parentId.has_value());
    EXPECT_EQ(inner.parentId.value(), outer.id);
    EXPECT_FALSE(outer.parentId.has_value());

    TelemetryContext::reset();
    {
        ScopedSpan later("later");
    }
    ASSERT_EQ(TelemetryContext::currentTrace().spans.size(), 1U);
    EXPECT_FALSE(TelemetryContext::currentTrace().spans.front().parentId.has_value());
}

TEST_F(ScopedSpanTest, AttributesAndImplicitOkStatusFinalize) {
    {
        ScopedSpan span("attrs");
        span.setAttribute("key", static_cast<std::int64_t>(42));
        span.setAttribute("path", std::string("foo.arxml"));
    }
    ASSERT_EQ(TelemetryContext::currentTrace().spans.size(), 1U);
    const auto& finished = TelemetryContext::currentTrace().spans.front();
    ASSERT_EQ(finished.attributes.size(), 2U);
    EXPECT_EQ(finished.attributes[0].key, "key");
    EXPECT_EQ(std::get<std::int64_t>(finished.attributes[0].value), 42);
    EXPECT_EQ(finished.attributes[1].key, "path");
    EXPECT_EQ(std::get<std::string>(finished.attributes[1].value), "foo.arxml");
    EXPECT_EQ(finished.status, SpanStatus::Ok);
}

TEST_F(ScopedSpanTest, ExceptionPathDefaultsToErrorStatus) {
    try {
        ScopedSpan span("throwing");
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    ASSERT_EQ(TelemetryContext::currentTrace().spans.size(), 1U);
    EXPECT_EQ(TelemetryContext::currentTrace().spans.front().status, SpanStatus::Error);
}

}  // namespace
