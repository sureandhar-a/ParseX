#include <gtest/gtest.h>
#include "parsex/raw/raw_node.hpp"

TEST(RawNodeTest,BuildTree)
{
    RawNode rootNode{.tagName = "node1", .parent = nullptr};
    rootNode.attributes.emplace_back("size","24");

    auto node2 = std::make_unique<RawNode>();
    node2->parent = &rootNode;
    node2->tagName = "child1";

    rootNode.children.push_back(std::move(node2));

    auto node3 = std::make_unique<RawNode>();
    node3->parent = &rootNode;
    rootNode.children.push_back(std::move(node3));

    EXPECT_EQ(rootNode.attributes.size(), 1);
    EXPECT_EQ(rootNode.children.size(), 2);
    EXPECT_EQ(rootNode.children.at(0)->tagName, "child1");
}