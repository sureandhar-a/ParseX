// Determinism test (PAR-150): identical logical models built with different
// insertion orders serialize byte-identically through the full
// tree-construction (PAR-139) + sorting (PAR-148/149) pipeline.

#include <gtest/gtest.h>

#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>
#include <parsex/write/write_context.hpp>
#include <parsex/write/write_elements.hpp>

#include <string>
#include <vector>

namespace {

const std::string kNs = "http://autosar.org/schema/r4.0";
const std::string kLocation = "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd";

// Builds the same logical model with element insertion driven by `order`:
// order[0..2] permute the three member signals; reversed flag flips channel
// and mapping insertion as a second axis of scrambling.
std::string buildModelDump(const std::vector<int>& order, bool reversed) {
    WriteContext ctx(kNs, kLocation);
    xmlNodePtr package = xmlNewChild(ctx.arPackages(), nullptr, BAD_CAST "AR-PACKAGE", nullptr);
    appendTextChild(package, "SHORT-NAME", "Sys");
    xmlNodePtr elements = xmlNewChild(package, nullptr, BAD_CAST "ELEMENTS", nullptr);

    const std::vector<std::string> signals = {"Sig_A", "Sig_B", "Sig_C"};
    SignalGroup group;
    group.common.shortName = "G";
    for (int idx : order) {
        group.members.push_back(signals.at(static_cast<std::size_t>(idx)));
    }

    Pdu pdu;
    pdu.common.shortName = "P";
    pdu.length = 8U;
    std::vector<PduSignalMapping> mappings;
    for (int idx : order) {
        mappings.push_back({.signalShortNameRef = signals.at(static_cast<std::size_t>(idx))});
    }
    pdu.signalMappings = reversed
                             ? std::vector<PduSignalMapping>(mappings.rbegin(), mappings.rend())
                             : mappings;

    Cluster cluster;
    cluster.common.shortName = "C";
    cluster.baudrate = 500000U;
    cluster.physicalChannels =
        reversed ? std::vector<std::string>{"can1", "can0"} : std::vector<std::string>{"can0", "can1"};

    xmlAddChild(elements, buildClusterElement(ctx.doc(), cluster));
    xmlAddChild(elements, buildPduElement(ctx.doc(), pdu));
    xmlAddChild(elements, buildSignalGroupElement(ctx.doc(), group));
    return ctx.dumpToString();
}

}  // namespace

TEST(DeterminismTest, SameModelDifferentInsertionOrdersSerializeIdentically) {
    // Two scrambles beyond the natural order: rotated and reversed.
    const std::string baseline = buildModelDump({0, 1, 2}, false);
    ASSERT_FALSE(baseline.empty());
    EXPECT_EQ(buildModelDump({2, 0, 1}, true), baseline);
    EXPECT_EQ(buildModelDump({1, 2, 0}, true), baseline);
}
