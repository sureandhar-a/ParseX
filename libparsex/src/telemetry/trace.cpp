#include <parsex/telemetry/trace.hpp>

#include <variant>

#include <parsex/json_contract/envelope.hpp>
#include <parsex/telemetry/span_status.hpp>

namespace parsex::telemetry {
namespace {

nlohmann::json attributeValueToJson(const AttributeValue& value) {
    return std::visit(
        [](const auto& held) -> nlohmann::json {
            return nlohmann::json(held);
        },
        value);
}

nlohmann::json spanToJson(const Span& span) {
    nlohmann::json spanJson;
    spanJson["spanId"] = span.id;
    // Omit-over-null: root spans carry no parentSpanId key at all, never null.
    if (span.parentId.has_value()) {
        spanJson["parentSpanId"] = span.parentId.value();
    }
    spanJson["name"] = span.name;
    spanJson["startNanos"] = span.startNanos;
    spanJson["endNanos"] = span.endNanos;
    spanJson["durationNanos"] =
        span.endNanos >= span.startNanos ? span.endNanos - span.startNanos : 0;
    spanJson["status"] = toString(span.status);
    nlohmann::json attrs = nlohmann::json::object();
    for (const auto& attr : span.attributes) {
        attrs[attr.key] = attributeValueToJson(attr.value);
    }
    spanJson["attributes"] = std::move(attrs);
    return spanJson;
}

}  // namespace

std::vector<const Span*> children(const Trace& trace, SpanId parent) {
    std::vector<const Span*> result;
    for (const auto& span : trace.spans) {
        if (span.parentId.has_value() && span.parentId.value() == parent) {
            result.push_back(&span);
        }
    }
    return result;
}

nlohmann::json Trace::toJson() const {
    nlohmann::json payload;
    payload["spans"] = nlohmann::json::array();
    for (const auto& span : spans) {
        payload["spans"].push_back(spanToJson(span));
    }
    return parsex::json_contract::wrapEnvelope("telemetryReport", std::move(payload));
}

}  // namespace parsex::telemetry
