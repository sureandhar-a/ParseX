// Schema-conformance tests for Trace::toJson() (PAR-195).
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>

#include <parsex/json_contract/schema_validate.hpp>
#include <parsex/telemetry/span.hpp>
#include <parsex/telemetry/trace.hpp>

namespace {

namespace fs = std::filesystem;

using parsex::telemetry::Attribute;
using parsex::telemetry::Span;
using parsex::telemetry::SpanStatus;
using parsex::telemetry::Trace;

fs::path envelopeSchema() {
#ifdef PARSEX_SCHEMAS_DIR
    return fs::path(PARSEX_SCHEMAS_DIR) / "envelope.schema.json";
#else
    return fs::path("schemas/envelope.schema.json");
#endif
}

Trace syntheticTrace() {
    Trace trace;
    Span root;
    root.name = "parse";
    root.id = 1;
    root.startNanos = 0;
    root.endNanos = 1500;
    root.status = SpanStatus::Ok;

    Span child;
    child.name = "tokenize";
    child.id = 2;
    child.parentId = root.id;
    child.startNanos = 100;
    child.endNanos = 900;
    child.attributes = {Attribute{"bytes", static_cast<std::int64_t>(9)},
                        Attribute{"ratio", 0.5}, Attribute{"cached", true},
                        Attribute{"path", std::string("a.arxml")}};
    child.status = SpanStatus::Ok;

    Span failing;
    failing.name = "validate";
    failing.id = 3;
    failing.parentId = root.id;
    failing.startNanos = 950;
    failing.endNanos = 1400;
    failing.status = SpanStatus::Error;

    trace.spans = {root, child, failing};
    return trace;
}

}  // namespace

TEST(TraceJsonTest, ToJsonValidatesAgainstEnvelopeSchema) {
    const Trace trace = syntheticTrace();
    const nlohmann::json doc = trace.toJson();
    ASSERT_EQ(doc.at("kind").get<std::string>(), "telemetryReport");
    std::string error;
    EXPECT_TRUE(
        parsex::json_contract::validatesAgainstSchema(doc, envelopeSchema(), &error))
        << error;
}

TEST(TraceJsonTest, RootOmitsParentSpanIdWhileChildIncludesIt) {
    const nlohmann::json doc = syntheticTrace().toJson();
    const auto& spans = doc.at("payload").at("spans");
    ASSERT_EQ(spans.size(), 3U);
    EXPECT_FALSE(spans[0].contains("parentSpanId")) << "root must omit parentSpanId, not null it";
    ASSERT_TRUE(spans[1].contains("parentSpanId"));
    EXPECT_EQ(spans[1].at("parentSpanId").get<std::uint64_t>(),
              spans[0].at("spanId").get<std::uint64_t>());
    EXPECT_EQ(spans[1].at("durationNanos").get<std::uint64_t>(), 800U);
    EXPECT_EQ(spans[2].at("status").get<std::string>(), "error");
    EXPECT_EQ(spans[1].at("attributes").at("bytes").get<std::int64_t>(), 9);
    EXPECT_EQ(spans[1].at("attributes").at("path").get<std::string>(), "a.arxml");
}
