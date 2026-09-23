// Unit tests for the Span/Trace data model (PAR-186).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <parsex/telemetry/attribute.hpp>
#include <parsex/telemetry/span.hpp>
#include <parsex/telemetry/span_id.hpp>
#include <parsex/telemetry/span_status.hpp>
#include <parsex/telemetry/trace.hpp>

namespace {

using parsex::telemetry::Attribute;
using parsex::telemetry::AttributeValue;
using parsex::telemetry::children;
using parsex::telemetry::nextSpanId;
using parsex::telemetry::Span;
using parsex::telemetry::SpanId;
using parsex::telemetry::SpanStatus;
using parsex::telemetry::toString;
using parsex::telemetry::Trace;

TEST(TelemetryDataModelTest, AttributeValueHoldsAllFourKinds) {
    AttributeValue fromString = std::string("file.arxml");
    EXPECT_TRUE(std::holds_alternative<std::string>(fromString));
    EXPECT_EQ(std::get<std::string>(fromString), "file.arxml");

    AttributeValue fromInt = static_cast<std::int64_t>(123456);
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(fromInt));
    EXPECT_EQ(std::get<std::int64_t>(fromInt), 123456);

    AttributeValue fromDouble = 3.25;
    EXPECT_TRUE(std::holds_alternative<double>(fromDouble));
    EXPECT_DOUBLE_EQ(std::get<double>(fromDouble), 3.25);

    AttributeValue fromBool = true;
    EXPECT_TRUE(std::holds_alternative<bool>(fromBool));
    EXPECT_EQ(std::get<bool>(fromBool), true);
}

TEST(TelemetryDataModelTest, ToStringCoversAllStatuses) {
    EXPECT_STREQ(toString(SpanStatus::Unset), "unset");
    EXPECT_STREQ(toString(SpanStatus::Ok), "ok");
    EXPECT_STREQ(toString(SpanStatus::Error), "error");
}

TEST(TelemetryDataModelTest, SpanFieldRoundTrip) {
    Span span;
    span.name = "parse";
    span.id = 42;
    span.parentId = static_cast<SpanId>(7);
    span.startNanos = 1000;
    span.endNanos = 2500;
    span.attributes = {Attribute{"file", std::string("a.arxml")},
                       Attribute{"bytes", static_cast<std::int64_t>(9)}};
    span.status = SpanStatus::Ok;

    EXPECT_EQ(span.name, "parse");
    EXPECT_EQ(span.id, 42U);
    ASSERT_TRUE(span.parentId.has_value());
    EXPECT_EQ(span.parentId.value(), 7U);
    EXPECT_EQ(span.startNanos, 1000U);
    EXPECT_EQ(span.endNanos, 2500U);
    ASSERT_EQ(span.attributes.size(), 2U);
    EXPECT_EQ(span.attributes[0].key, "file");
    EXPECT_EQ(std::get<std::string>(span.attributes[0].value), "a.arxml");
    EXPECT_EQ(span.attributes[1].key, "bytes");
    EXPECT_EQ(std::get<std::int64_t>(span.attributes[1].value), 9);
    EXPECT_EQ(span.status, SpanStatus::Ok);
}

TEST(TelemetryDataModelTest, NextSpanIdIsUniqueSequentiallyAndConcurrently) {
    std::vector<SpanId> sequential;
    sequential.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        sequential.push_back(nextSpanId());
    }
    EXPECT_TRUE(std::is_sorted(sequential.begin(), sequential.end()));
    EXPECT_EQ(std::unordered_set<SpanId>(sequential.begin(), sequential.end()).size(),
              sequential.size());

    constexpr int kThreads = 8;
    constexpr int kPerThread = 1000;
    std::vector<std::vector<SpanId>> perThread(kThreads);
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([t, &perThread]() {
            perThread[t].reserve(kPerThread);
            for (int i = 0; i < kPerThread; ++i) {
                perThread[t].push_back(nextSpanId());
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    std::unordered_set<SpanId> concurrent;
    for (const auto& ids : perThread) {
        concurrent.insert(ids.begin(), ids.end());
    }
    EXPECT_EQ(concurrent.size(), static_cast<std::size_t>(kThreads * kPerThread));
}

TEST(TelemetryDataModelTest, ChildrenFindsDirectChildrenInInsertionOrder) {
    Trace trace;
    Span root;
    root.name = "root";
    root.id = nextSpanId();
    Span firstChild;
    firstChild.name = "child1";
    firstChild.id = nextSpanId();
    firstChild.parentId = root.id;
    Span secondChild;
    secondChild.name = "child2";
    secondChild.id = nextSpanId();
    secondChild.parentId = root.id;
    Span grandchild;
    grandchild.name = "grandchild";
    grandchild.id = nextSpanId();
    grandchild.parentId = firstChild.id;

    const SpanId rootId = root.id;
    const SpanId firstChildId = firstChild.id;
    trace.spans = {root, firstChild, secondChild, grandchild};

    const std::vector<const Span*> rootChildren = children(trace, rootId);
    ASSERT_EQ(rootChildren.size(), 2U);
    EXPECT_EQ(rootChildren[0]->name, "child1");
    EXPECT_EQ(rootChildren[1]->name, "child2");

    const std::vector<const Span*> nested = children(trace, firstChildId);
    ASSERT_EQ(nested.size(), 1U);
    EXPECT_EQ(nested[0]->name, "grandchild");
}

}  // namespace
