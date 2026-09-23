// Whitespace-guard tests (PAR-151): the tree built from PAR-139's mixed
// fixture contains no whitespace-only text nodes; a deliberately-injected
// one is detected.

#include <gtest/gtest.h>

#include <parsex/model/cluster.hpp>
#include <parsex/write/write_context.hpp>
#include <parsex/write/write_elements.hpp>
#include <parsex/write/write_format.hpp>

TEST(WhitespaceGuardTest, CleanTreePasses) {
    WriteContext ctx("http://autosar.org/schema/r4.0",
                     "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd");
    Cluster cluster;
    cluster.common.shortName = "CAN_Cluster";
    cluster.baudrate = 500000U;
    cluster.physicalChannels = {"can0"};
    xmlNodePtr node = buildClusterElement(ctx.doc(), cluster);
    xmlAddChild(ctx.arPackages(), node);
    EXPECT_NO_THROW(assertNoWhitespaceOnlyTextNodes(ctx.root()));
}

TEST(WhitespaceGuardTest, InjectedWhitespaceNodeFails) {
    WriteContext ctx("http://autosar.org/schema/r4.0",
                     "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd");
    xmlNodePtr ws = xmlNewText(BAD_CAST "   \n  ");
    ASSERT_NE(ws, nullptr);
    xmlAddChild(ctx.arPackages(), ws);
    EXPECT_THROW(assertNoWhitespaceOnlyTextNodes(ctx.root()), std::runtime_error);
}
