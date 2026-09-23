// Facade tests for TelemetryContext (PAR-199).
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <parsex/json_contract/schema_validate.hpp>
#include <parsex/telemetry/scoped_span_macro.hpp>
#include <parsex/telemetry/telemetry_config.hpp>
#include <parsex/telemetry/telemetry_context.hpp>

namespace {

namespace fs = std::filesystem;

using parsex::telemetry::TelemetryConfig;
using parsex::telemetry::TelemetryContext;

fs::path envelopeSchema() {
#ifdef PARSEX_SCHEMAS_DIR
    return fs::path(PARSEX_SCHEMAS_DIR) / "envelope.schema.json";
#else
    return fs::path("schemas/envelope.schema.json");
#endif
}

void runNestedOperations() {
    PARSEX_SPAN("run");
    {
        PARSEX_SPAN("phaseA");
    }
    {
        PARSEX_SPAN("phaseB");
    }
}

}  // namespace

TEST(TelemetryContextTest, EnabledRunProducesValidNestedJson) {
    TelemetryConfig::setEnabled(true);
    TelemetryContext::reset();
    runNestedOperations();

    ASSERT_EQ(TelemetryContext::currentTrace().spans.size(), 3U);
    const nlohmann::json doc = TelemetryContext::currentTraceAsJson();
    EXPECT_EQ(doc.at("kind").get<std::string>(), "telemetryReport");
    std::string error;
    EXPECT_TRUE(
        parsex::json_contract::validatesAgainstSchema(doc, envelopeSchema(), &error))
        << error;

    const auto& spans = doc.at("payload").at("spans");
    ASSERT_EQ(spans.size(), 3U);
    // Append order: phaseA, phaseB, run (destructors finalize children first).
    EXPECT_EQ(spans[2].at("name").get<std::string>(), "run");
    EXPECT_FALSE(spans[2].contains("parentSpanId"));
    const auto runId = spans[2].at("spanId").get<std::uint64_t>();
    EXPECT_EQ(spans[0].at("parentSpanId").get<std::uint64_t>(), runId);
    EXPECT_EQ(spans[1].at("parentSpanId").get<std::uint64_t>(), runId);

    TelemetryConfig::setEnabled(false);
    TelemetryContext::reset();
}

TEST(TelemetryContextTest, DisabledRunLeavesTraceEmpty) {
    TelemetryConfig::setEnabled(false);
    TelemetryContext::reset();
    runNestedOperations();
    EXPECT_TRUE(TelemetryContext::currentTrace().spans.empty());
    const nlohmann::json doc = TelemetryContext::currentTraceAsJson();
    EXPECT_EQ(doc.at("payload").at("spans").size(), 0U);
}
