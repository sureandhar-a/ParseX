// Tree-construction tests (PAR-147): consolidates PAR-139's three PBI-level
// "Unit tests" scenarios into one dedicated file wired into CTest.

#include <gtest/gtest.h>

#include <libxml/tree.h>

#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>
#include <parsex/write/write_context.hpp>
#include <parsex/write/write_elements.hpp>

namespace {

const xmlNode* findChild(const xmlNode* parent, const char* tag) {
    for (const xmlNode* child = parent->children; child != nullptr; child = child->next) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, BAD_CAST tag) == 0) {
            return child;
        }
    }
    return nullptr;
}

std::string propValue(const xmlNode* node, const char* name) {
    xmlChar* raw = xmlGetProp(const_cast<xmlNode*>(node), BAD_CAST name);
    if (raw == nullptr) {
        return "";
    }
    std::string out(reinterpret_cast<const char*>(raw));
    xmlFree(raw);
    return out;
}

std::string nsPropValue(const xmlNode* node, const char* nsHref, const char* name) {
    xmlChar* raw =
        xmlGetNsProp(const_cast<xmlNode*>(node), BAD_CAST name, BAD_CAST nsHref);
    if (raw == nullptr) {
        return "";
    }
    std::string out(reinterpret_cast<const char*>(raw));
    xmlFree(raw);
    return out;
}

int countChildren(const xmlNode* parent, const char* tag) {
    int count = 0;
    for (const xmlNode* child = parent->children; child != nullptr; child = child->next) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, BAD_CAST tag) == 0) {
            ++count;
        }
    }
    return count;
}

Cluster makeCluster() {
    Cluster cluster;
    cluster.common.shortName = "CAN_Cluster";
    cluster.baudrate = 500000U;
    cluster.physicalChannels = {"can0"};
    return cluster;
}

}  // namespace

TEST(TreeConstructionTest, MixedFixtureRootAndPackages) {
    WriteContext ctx("http://autosar.org/schema/r4.0",
                     "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd");

    // Attach one of each domain type under AR-PACKAGE/ELEMENTS (the nesting
    // the Parser expects — see parsefile_complete.arxml).
    xmlNodePtr package = xmlNewChild(ctx.arPackages(), nullptr, BAD_CAST "AR-PACKAGE", nullptr);
    ASSERT_NE(package, nullptr);
    appendTextChild(package, "SHORT-NAME", "Sys");
    xmlNodePtr elements = xmlNewChild(package, nullptr, BAD_CAST "ELEMENTS", nullptr);
    ASSERT_NE(elements, nullptr);

    Cluster cluster = makeCluster();
    EcuInstance ecu;
    ecu.common.shortName = "ECU_A";
    Frame frame;
    frame.common.shortName = "Frame_1";
    frame.length = 8U;
    Pdu pdu;
    pdu.common.shortName = "Pdu_1";
    pdu.length = 8U;
    Signal signal;
    signal.common.shortName = "Signal_1";
    signal.bitLength = 16U;

    xmlAddChild(elements, buildClusterElement(ctx.doc(), cluster));
    xmlAddChild(elements, buildEcuInstanceElement(ctx.doc(), ecu));
    xmlAddChild(elements, buildFrameElement(ctx.doc(), frame));
    xmlAddChild(elements, buildPduElement(ctx.doc(), pdu));
    xmlAddChild(elements, buildSignalElement(ctx.doc(), signal));

    // Root element name, namespace URI, and AR-PACKAGES child are correct.
    EXPECT_STREQ(reinterpret_cast<const char*>(ctx.root()->name), "AUTOSAR");
    ASSERT_NE(ctx.root()->ns, nullptr);
    EXPECT_STREQ(reinterpret_cast<const char*>(ctx.root()->ns->href),
                 "http://autosar.org/schema/r4.0");
    const xmlNode* arPackages = findChild(ctx.root(), "AR-PACKAGES");
    ASSERT_NE(arPackages, nullptr);
    EXPECT_EQ(arPackages, ctx.arPackages());

    // Manual one-time check (documented, not asserted): dump reads as
    // visibly well-formed AUTOSAR XML via xmlDocDumpFormatMemory.
    // const std::string dump = ctx.dumpToString();
    // printf("%s\n", dump.c_str());
}

TEST(TreeConstructionTest, SchemaLocationMatchesConfigured) {
    const std::string ns = "http://autosar.org/schema/r4.0";
    const std::string location = ns + " AUTOSAR_00046.xsd";
    WriteContext ctx(ns, location);
    EXPECT_EQ(nsPropValue(ctx.root(), "http://www.w3.org/2001/XMLSchema-instance",
                          "schemaLocation"),
              location);
}

TEST(TreeConstructionTest, SignalGroupWithTwoMembers) {
    WriteContext ctx("http://autosar.org/schema/r4.0",
                     "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd");
    SignalGroup group;
    group.common.shortName = "SignalGroup_1";
    group.members = {"Signal_1", "Signal_2"};
    xmlNodePtr node = buildSignalGroupElement(ctx.doc(), group);
    ASSERT_NE(node, nullptr);
    const xmlNode* members = findChild(node, "I-SIGNAL-REFS");
    ASSERT_NE(members, nullptr);
    EXPECT_EQ(countChildren(members, "I-SIGNAL-REF"), 2);
    xmlFreeNode(node);
}
