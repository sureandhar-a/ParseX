// Loader tests: the RawNode tree's shape must match the fixture XML, and —
// critically — each span must slice the real file bytes back to the exact
// element text. A span silently off by a few bytes would pass "numbers look
// reasonable" checks and only explode later in the Write Engine's splicing,
// so these tests compare actual substrings.

#include <gtest/gtest.h>

#include <parsex/parser/loader.hpp>
#include <parsex/raw/raw_span.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

std::string readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string slice(const std::string& bytes, const RawSpan& span) {
    return bytes.substr(span.startOffset, span.endOffset - span.startOffset);
}

const RawNode& childAt(const RawNode& node, std::size_t idx) {
    return *node.children.at(idx);
}

const RawNode* findChild(const RawNode& node, const std::string& tag) {
    for (const auto& child : node.children) {
        if (child->tagName == tag) {
            return child.get();
        }
    }
    ADD_FAILURE() << "no child <" << tag << "> under <" << node.tagName << ">";
    return nullptr;
}

std::string attrValue(const RawNode& node, const std::string& key) {
    for (const auto& attr : node.attributes) {
        if (attr.first == key) {
            return attr.second;
        }
    }
    ADD_FAILURE() << "no attribute @" << key << " on <" << node.tagName << ">";
    return "";
}

std::size_t countElements(const RawNode& node) {
    std::size_t total = 1;
    for (const auto& child : node.children) {
        total += countElements(*child);
    }
    return total;
}

void expectParentLinks(const RawNode& node) {
    for (const auto& child : node.children) {
        EXPECT_EQ(child->parent, &node) << "broken parent link on <" << child->tagName << ">";
        expectParentLinks(*child);
    }
}

const RawNode* findDescendant(const RawNode& node, const std::string& tag) {
    if (node.tagName == tag) {
        return &node;
    }
    for (const auto& child : node.children) {
        const RawNode* found = findDescendant(*child, tag);
        if (found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

}  // namespace

TEST(LoaderTest, BasicFixtureTreeShape) {
    const RawDocument doc = loadRawDocument(fixture("loader_basic.arxml"));

    ASSERT_EQ(doc.root.tagName, "AUTOSAR");
    EXPECT_EQ(doc.root.parent, nullptr);
    EXPECT_EQ(attrValue(doc.root, "xmlns"), "http://autosar.org/schema/r4.0");
    ASSERT_EQ(doc.root.children.size(), 1U);

    const RawNode& packages = childAt(doc.root, 0);
    EXPECT_EQ(packages.tagName, "AR-PACKAGES");
    ASSERT_EQ(packages.children.size(), 1U);

    const RawNode& pkg = childAt(packages, 0);
    EXPECT_EQ(pkg.tagName, "AR-PACKAGE");
    EXPECT_EQ(attrValue(pkg, "UUID"), "pkg-1");
    ASSERT_EQ(pkg.children.size(), 2U);

    const RawNode& shortName = childAt(pkg, 0);
    EXPECT_EQ(shortName.tagName, "SHORT-NAME");
    EXPECT_TRUE(shortName.children.empty());

    const RawNode& elements = childAt(pkg, 1);
    EXPECT_EQ(elements.tagName, "ELEMENTS");
    ASSERT_EQ(elements.children.size(), 1U);
    EXPECT_EQ(childAt(elements, 0).tagName, "CLUSTER");
    const RawNode* clusterShortName = findChild(childAt(elements, 0), "SHORT-NAME");
    ASSERT_NE(clusterShortName, nullptr);
    EXPECT_EQ(clusterShortName->tagName, "SHORT-NAME");

    expectParentLinks(doc.root);
}

TEST(LoaderTest, BasicFixtureByteSlices) {
    const auto path = fixture("loader_basic.arxml");
    const std::string bytes = readBytes(path);
    const RawDocument doc = loadRawDocument(path);

    // Root slice is exactly the root element text: it starts at `<AUTOSAR>`
    // (the XML declaration is not part of any element span) and runs to just
    // past the closing `>` (the file's trailing newline is outside the span).
    EXPECT_EQ(doc.root.span.startOffset, 39U);
    EXPECT_EQ(slice(bytes, doc.root.span),
              "<AUTOSAR xmlns=\"http://autosar.org/schema/r4.0\">\n"
              "  <AR-PACKAGES>\n"
              "    <AR-PACKAGE UUID=\"pkg-1\">\n"
              "      <SHORT-NAME>CanCluster</SHORT-NAME>\n"
              "      <ELEMENTS>\n"
              "        <CLUSTER>\n"
              "          <SHORT-NAME>CAN_1</SHORT-NAME>\n"
              "        </CLUSTER>\n"
              "      </ELEMENTS>\n"
              "    </AR-PACKAGE>\n"
              "  </AR-PACKAGES>\n"
              "</AUTOSAR>");
    EXPECT_EQ(doc.root.span.endOffset + 1, bytes.size());
    EXPECT_EQ(doc.root.span.lineNumber, 2U);

    const RawNode& pkg = childAt(childAt(doc.root, 0), 0);
    EXPECT_EQ(pkg.span.startOffset, 108U);
    EXPECT_EQ(pkg.span.lineNumber, 4U);
    EXPECT_EQ(slice(bytes, pkg.span),
              "<AR-PACKAGE UUID=\"pkg-1\">\n"
              "      <SHORT-NAME>CanCluster</SHORT-NAME>\n"
              "      <ELEMENTS>\n"
              "        <CLUSTER>\n"
              "          <SHORT-NAME>CAN_1</SHORT-NAME>\n"
              "        </CLUSTER>\n"
              "      </ELEMENTS>\n"
              "    </AR-PACKAGE>");

    const RawNode& shortName = childAt(pkg, 0);
    EXPECT_EQ(slice(bytes, shortName.span), "<SHORT-NAME>CanCluster</SHORT-NAME>");
    EXPECT_EQ(shortName.span.lineNumber, 5U);

    const RawNode& cluster = childAt(childAt(pkg, 1), 0);
    EXPECT_EQ(cluster.tagName, "CLUSTER");
    EXPECT_EQ(slice(bytes, cluster.span),
              "<CLUSTER>\n"
              "          <SHORT-NAME>CAN_1</SHORT-NAME>\n"
              "        </CLUSTER>");
    EXPECT_EQ(cluster.span.lineNumber, 7U);
}

TEST(LoaderTest, NestedFixtureSiblingsAndDepth) {
    const auto path = fixture("loader_nested.arxml");
    const std::string bytes = readBytes(path);
    const RawDocument doc = loadRawDocument(path);

    ASSERT_EQ(doc.root.tagName, "SYSTEM");
    ASSERT_EQ(doc.root.children.size(), 2U);

    const RawNode& ecus = childAt(doc.root, 0);
    EXPECT_EQ(ecus.tagName, "ECUS");
    ASSERT_EQ(ecus.children.size(), 2U);

    const RawNode& ecuA = childAt(ecus, 0);
    EXPECT_EQ(attrValue(ecuA, "ID"), "a");
    ASSERT_EQ(ecuA.children.size(), 2U);
    EXPECT_EQ(childAt(ecuA, 0).tagName, "SHORT-NAME");

    // Self-closed tag: span is exactly the `<EMPTY/>` text.
    const RawNode& empty = childAt(ecuA, 1);
    EXPECT_EQ(empty.tagName, "EMPTY");
    EXPECT_TRUE(empty.children.empty());
    EXPECT_EQ(empty.span.startOffset, 126U);
    EXPECT_EQ(slice(bytes, empty.span), "<EMPTY/>");
    EXPECT_EQ(empty.span.lineNumber, 6U);

    const RawNode& ecuB = childAt(ecus, 1);
    EXPECT_EQ(attrValue(ecuB, "ID"), "b");
    const RawNode* channels = findChild(ecuB, "CHANNELS");
    ASSERT_NE(channels, nullptr);
    ASSERT_EQ(channels->children.size(), 2U);
    EXPECT_EQ(childAt(*channels, 0).tagName, "CHANNEL");
    EXPECT_EQ(childAt(*channels, 1).tagName, "CHANNEL");

    // Deep sibling at depth 4 slices exactly (stack survived the subtree).
    const RawNode& secondChannel = childAt(*channels, 1);
    EXPECT_EQ(secondChannel.span.startOffset, 274U);
    EXPECT_EQ(slice(bytes, secondChannel.span), "<CHANNEL>ch1</CHANNEL>");
    EXPECT_EQ(secondChannel.span.lineNumber, 12U);

    // Element after a deep subtree: the stack popped correctly.
    const RawNode& tail = childAt(doc.root, 1);
    EXPECT_EQ(tail.tagName, "TAIL");
    EXPECT_EQ(slice(bytes, tail.span), "<TAIL>done</TAIL>");
    EXPECT_EQ(tail.span.lineNumber, 16U);

    // Full second-subtree slice pins nesting endOffsets, not just starts.
    EXPECT_EQ(slice(bytes, ecuB.span),
              "<ECU-INSTANCE ID=\"b\">\n"
              "      <SHORT-NAME>ECU_B</SHORT-NAME>\n"
              "      <CHANNELS>\n"
              "        <CHANNEL>ch0</CHANNEL>\n"
              "        <CHANNEL>ch1</CHANNEL>\n"
              "      </CHANNELS>\n"
              "    </ECU-INSTANCE>");

    expectParentLinks(doc.root);
}

TEST(LoaderTest, RealWorldFileLoadsWithExactStructure) {
    const auto path = fixture("system-4.2.arxml");
    const RawDocument doc = loadRawDocument(path);

    // Independent inventory of the file says 918 elements; the tree must hold
    // every one of them (see system-4.2.arxml.provenance).
    EXPECT_EQ(countElements(doc.root), 918U);
    EXPECT_EQ(doc.root.tagName, "AUTOSAR");

    // UTF-8 BOM (3 bytes) + 39-byte declaration pin the root span.
    const std::string bytes = readBytes(path);
    EXPECT_EQ(doc.root.span.startOffset, 42U);
    EXPECT_EQ(doc.root.span.endOffset + 1, bytes.size());
    EXPECT_EQ(slice(bytes, doc.root.span), bytes.substr(42, bytes.size() - 43));
}

TEST(LoaderTest, CharacterDataCapturedAsNodeText) {
    const RawDocument basic = loadRawDocument(fixture("loader_basic.arxml"));
    const RawNode* shortName = findDescendant(basic.root, "SHORT-NAME");
    ASSERT_NE(shortName, nullptr);
    EXPECT_EQ(shortName->text, "CanCluster");

    // Self-closed tags see no character callbacks: text stays empty.
    const RawDocument nested = loadRawDocument(fixture("loader_nested.arxml"));
    const RawNode* empty = findDescendant(nested.root, "EMPTY");
    ASSERT_NE(empty, nullptr);
    EXPECT_EQ(empty->text, "");
}

TEST(LoaderTest, MissingFileThrows) {    EXPECT_THROW(loadRawDocument(fixture("does-not-exist.arxml")), std::runtime_error);
}

TEST(LoaderTest, IllFormedFileThrows) {
    const auto path = std::filesystem::temp_directory_path() / "parsex_loader_bad.arxml";
    {
        std::ofstream out(path, std::ios::binary);
        out << "<A><B></A>";
    }
    EXPECT_THROW(loadRawDocument(path), std::runtime_error);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
