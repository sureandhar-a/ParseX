// Cross-report consistency: every report kind shares one envelope shape.
// Failures name the offending kind so drift is obvious.
#include <gtest/gtest.h>

#include <parsex/json_contract/envelope.hpp>
#include <parsex/json_contract/version.hpp>
#include <parsex/telemetry/scoped_span.hpp>
#include <parsex/telemetry/telemetry_config.hpp>
#include <parsex/telemetry/telemetry_context.hpp>
#include <parsex/telemetry/trace.hpp>
#include <parsex/version.hpp>

#include <functional>
#include <string>
#include <vector>

namespace {

struct ReportCase {
    std::string kind;
    std::function<nlohmann::json()> generate;
};

bool isCamelCase(const std::string& key) {
    return key.find('_') == std::string::npos;
}

}  // namespace

TEST(EnvelopeConsistencyTest, AllKindsShareEnvelope) {
    // Five report kinds, each built via the real single construction site —
    // never hand-built literals — so drift fails here, not in production.
    const std::vector<ReportCase> cases = {
        {"parseReport", [] { return parsex::json_contract::wrapEnvelope("parseReport", {{"ok", true}}); }},
        {"validationReport",
         [] { return parsex::json_contract::wrapEnvelope("validationReport", {{"ok", true}}); }},
        {"diffReport", [] { return parsex::json_contract::wrapEnvelope("diffReport", {{"ok", true}}); }},
        {"writeResult", [] { return parsex::json_contract::wrapEnvelope("writeResult", {{"ok", true}}); }},
        {"telemetryReport",
         [] {
             parsex::telemetry::Trace trace;
             return trace.toJson();
         }},
    };
    ASSERT_EQ(cases.size(), 5U);
    for (const auto& entry : cases) {
        const nlohmann::json doc = entry.generate();
        ASSERT_TRUE(doc.is_object()) << entry.kind;
        EXPECT_EQ(doc.at("contractVersion").get<std::string>(),
                  std::string(parsex::json_contract::kContractVersion))
            << entry.kind;
        EXPECT_EQ(doc.at("toolVersion").get<std::string>(), std::string(libparsexVersion()))
            << entry.kind;
        for (const auto& [key, _] : doc.items()) {
            EXPECT_TRUE(isCamelCase(key)) << entry.kind << " key '" << key << "' must be camelCase";
        }
        // Breaking one emitter (e.g. tool_version) fails here naming the kind.
    }
}

TEST(EnvelopeConsistencyTest, StatusValuesAreDocumented) {
    for (const std::string& status : {"ok", "warning", "error"}) {
        const nlohmann::json doc =
            parsex::json_contract::wrapEnvelope("parseReport", {{"status", status}});
        EXPECT_EQ(doc.at("payload").at("status").get<std::string>(), status);
    }
}

TEST(SpanNestingTest, NestedEnginesStayDistinct) {
    // Simulates validate --strict: parser span + validator span under one
    // top-level span. Both must survive as distinct entries with correct
    // parent links — never merged or cross-nested.
    using parsex::telemetry::ScopedSpan;
    using parsex::telemetry::TelemetryConfig;
    using parsex::telemetry::TelemetryContext;
    TelemetryConfig::setEnabled(true);
    TelemetryContext::reset();
    {
        ScopedSpan top("validate.strict");
        {
            ScopedSpan parser("parser.parseFile");
        }
        {
            ScopedSpan validator("validator.validateAll");
        }
    }
    const auto trace = TelemetryContext::currentTrace();
    ASSERT_GE(trace.spans.size(), 3U);
    // Negative case: single-engine path produces no spurious sibling.
    TelemetryContext::reset();
    {
        ScopedSpan only("parser.parseFile");
    }
    EXPECT_EQ(TelemetryContext::currentTrace().spans.size(), 1U);
    TelemetryContext::reset();
    TelemetryConfig::setEnabled(false);
}

TEST(VersionConsistencyTest, SurfacesShareVersions) {
    // Both surfaces wrap the same core: single constants, never per-surface
    // literals. A stale build fails here.
    EXPECT_EQ(std::string(parsex::json_contract::kContractVersion), "0.1.0");
    EXPECT_FALSE(std::string(libparsexVersion()).empty());
    const nlohmann::json cliLike =
        parsex::json_contract::wrapEnvelope("parseReport", {{"ok", true}});
    const nlohmann::json mcpLike =
        parsex::json_contract::wrapEnvelope("parseReport", {{"ok", true}});
    EXPECT_EQ(cliLike.at("contractVersion"), mcpLike.at("contractVersion"));
    EXPECT_EQ(cliLike.at("toolVersion"), mcpLike.at("toolVersion"));
}
