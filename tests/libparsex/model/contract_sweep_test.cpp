// Shared-type guarantees swept in one place: every domain object carrying a
// location must have a valid non-empty range, and every domain object must
// stay copyable. Add new types with one line in the lists below.
#include <gtest/gtest.h>

#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>
#include <parsex/parser/parser.hpp>

#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

namespace {

// One entry per domain type: add future types here, nothing else to change.
struct SpanCase {
    std::string typeName;
    std::vector<RawSpan> spans;
};

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

}  // namespace

TEST(DomainContractTest, AllSpansAreValid) {
    const ParsedFile file = Parser{}.parseFile(fixture("parsefile_complete.arxml"));
    std::vector<SpanCase> cases;
    for (const auto& c : file.clusters) cases.push_back({"Cluster", {c.common.rawSpanRef}});
    for (const auto& e : file.ecuInstances) cases.push_back({"EcuInstance", {e.common.rawSpanRef}});
    for (const auto& f : file.frames) cases.push_back({"Frame", {f.common.rawSpanRef}});
    for (const auto& p : file.pdus) cases.push_back({"Pdu", {p.common.rawSpanRef}});
    for (const auto& s : file.signals) cases.push_back({"Signal", {s.common.rawSpanRef}});
    for (const auto& g : file.signalGroups)
        cases.push_back({"SignalGroup", {g.common.rawSpanRef}});
    ASSERT_FALSE(cases.empty()) << "fixture must contain domain objects";
    for (const auto& entry : cases) {
        for (const auto& span : entry.spans) {
            EXPECT_TRUE(span.isValid())
                << entry.typeName << " has invalid span [" << span.startOffset << ", "
                << span.endOffset << ")";
        }
    }
}

TEST(DomainContractTest, AllTypesAreCopyable) {
    // Compile-time + runtime: copy construction preserves short names.
    static_assert(std::is_copy_constructible_v<Cluster>);
    static_assert(std::is_copy_constructible_v<EcuInstance>);
    static_assert(std::is_copy_constructible_v<Frame>);
    static_assert(std::is_copy_constructible_v<Pdu>);
    static_assert(std::is_copy_constructible_v<Signal>);
    static_assert(std::is_copy_constructible_v<SignalGroup>);
    const ParsedFile file = Parser{}.parseFile(fixture("parsefile_complete.arxml"));
    ASSERT_FALSE(file.clusters.empty());
    const Cluster copy = file.clusters.front();
    EXPECT_EQ(copy.common.shortName, file.clusters.front().common.shortName);
}
