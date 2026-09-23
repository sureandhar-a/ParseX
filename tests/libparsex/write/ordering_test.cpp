// Attribute-sorting tests (PAR-148): TPS_ASR_00019 — attributes emitted in
// strict alphabetical order regardless of insertion order; root namespace
// declarations and xsi:schemaLocation keep their conventional position.

#include <gtest/gtest.h>

#include <libxml/tree.h>

#include <parsex/write/write_context.hpp>
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
