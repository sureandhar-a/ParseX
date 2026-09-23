// Attribute-sorting tests (PAR-148): TPS_ASR_00019 — attributes emitted in
// strict alphabetical order regardless of insertion order; root namespace
// declarations and xsi:schemaLocation keep their conventional position.

#include <gtest/gtest.h>

#include <libxml/tree.h>

#include <parsex/model/pdu.hpp>
#include <parsex/model/signal_group.hpp>
#include <parsex/write/write_context.hpp>
#include <parsex/write/write_elements.hpp>
#include <parsex/write/write_ordering.hpp>

#include <string>
#include <vector>

namespace {

std::vector<std::string> propNames(xmlNodePtr node) {
    std::vector<std::string> names;
    for (xmlAttrPtr attr = node->properties; attr != nullptr; attr = attr->next) {
        names.emplace_back(reinterpret_cast<const char*>(attr->name));
    }
    return names;
}

}  // namespace

TEST(AttributeSortTest, ScrambledAttributesSerializeAlphabetically) {
    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
    ASSERT_NE(doc, nullptr);
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "TEST-ELEMENT");
    ASSERT_NE(node, nullptr);
    xmlDocSetRootElement(doc, node);

    setSortedAttributes(node, {{"z-attr", "1"}, {"a-attr", "2"}, {"m-attr", "3"}});
    EXPECT_EQ(propNames(node), std::vector<std::string>({"a-attr", "m-attr", "z-attr"}));
    xmlFreeDoc(doc);
}

TEST(AttributeSortTest, RootNamespacesKeepConventionalPosition) {
    const std::string ns = "http://autosar.org/schema/r4.0";
    WriteContext ctx(ns, ns + " AUTOSAR_00046.xsd");
    // Namespace declarations and schemaLocation unaffected by the sort
    // applied to ordinary domain-type attributes.
    ASSERT_NE(ctx.root()->ns, nullptr);
    EXPECT_STREQ(reinterpret_cast<const char*>(ctx.root()->ns->href), ns.c_str());
    xmlChar* location =
        xmlGetNsProp(ctx.root(), BAD_CAST "schemaLocation",
                     BAD_CAST "http://www.w3.org/2001/XMLSchema-instance");
    ASSERT_NE(location, nullptr);
    EXPECT_STREQ(reinterpret_cast<const char*>(location), (ns + " AUTOSAR_00046.xsd").c_str());
    xmlFree(location);
    // Only the conventional schemaLocation sits on the root — no ordinary
    // alphabetized domain attributes leaked onto it.
    EXPECT_EQ(propNames(ctx.root()), std::vector<std::string>({"schemaLocation"}));
}

// Short-name sorting tests (PAR-149): TPS_ASR_00014 — semantically-unordered
// repeated elements serialize in short-name order; ordered collections are
// preserved exactly.

namespace {

std::vector<std::string> refTexts(const xmlNode* parent, const char* wrapper,
                                  const char* refTag) {
    const xmlNode* box = nullptr;
    for (const xmlNode* child = parent->children; child != nullptr; child = child->next) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, BAD_CAST wrapper) == 0) {
            box = child;
            break;
        }
    }
    if (box == nullptr) {
        return {};
    }
    std::vector<std::string> texts;
    for (const xmlNode* child = box->children; child != nullptr; child = child->next) {
        if (child->type != XML_ELEMENT_NODE) {
            continue;
        }
        for (const xmlNode* ref = child->children; ref != nullptr; ref = ref->next) {
            if (ref->type == XML_ELEMENT_NODE && xmlStrcmp(ref->name, BAD_CAST refTag) == 0) {
                xmlChar* content = xmlNodeGetContent(ref);
                texts.emplace_back(reinterpret_cast<const char*>(content));
                xmlFree(content);
            }
        }
        // Direct ref (SignalGroup MEMBERS holds refs straight under MEMBERS).
        if (xmlStrcmp(child->name, BAD_CAST refTag) == 0) {
            xmlChar* content = xmlNodeGetContent(child);
            texts.emplace_back(reinterpret_cast<const char*>(content));
            xmlFree(content);
        }
    }
    return texts;
}

}  // namespace

TEST(ElementSortTest, SignalGroupMembersSerializeInShortNameOrder) {
    WriteContext ctx("http://autosar.org/schema/r4.0",
                     "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd");
    SignalGroup group;
    group.common.shortName = "G";
    group.members = {"Signal_C", "Signal_A", "Signal_B"};
    xmlNodePtr node = buildSignalGroupElement(ctx.doc(), group);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(refTexts(node, "MEMBERS", "SYSTEM-SIGNAL-REF"),
              std::vector<std::string>({"/Sys/Signal_A", "/Sys/Signal_B", "/Sys/Signal_C"}));
    xmlFreeNode(node);
}

TEST(ElementSortTest, PduSignalMappingsSerializeInShortNameOrder) {
    WriteContext ctx("http://autosar.org/schema/r4.0",
                     "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd");
    Pdu pdu;
    pdu.common.shortName = "P";
    pdu.length = 8U;
    pdu.signalMappings = {
        {.signalShortNameRef = "Sig_C"}, {.signalShortNameRef = "Sig_A"},
        {.signalShortNameRef = "Sig_B"}};
    xmlNodePtr node = buildPduElement(ctx.doc(), pdu);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(refTexts(node, "SIGNAL-MAPPINGS", "SIGNAL-REF"),
              std::vector<std::string>({"/Sys/Sig_A", "/Sys/Sig_B", "/Sys/Sig_C"}));
    xmlFreeNode(node);
}

TEST(ElementSortTest, OrderedCollectionIsNotResorted) {
    // No v1 domain collection is ordered (see write_ordering.hpp) — this pins
    // the ordered branch itself: preserveElementOrder keeps original order
    // exactly, proving classification branches rather than sorting everything.
    EXPECT_EQ(preserveElementOrder<std::string>({"c", "a", "b"}),
              std::vector<std::string>({"c", "a", "b"}));
}
