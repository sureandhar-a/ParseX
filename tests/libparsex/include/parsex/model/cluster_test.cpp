#include <gtest/gtest.h>
#include <parsex/model/cluster.hpp>
#include <parsex/raw/raw_span.hpp>

TEST(ClusterTest, DefaultConstructionInitializesEmptyFields)
{
    Cluster cluster{};

    EXPECT_EQ(cluster.common.shortName, "");
    EXPECT_EQ(cluster.common.category, std::nullopt);
    EXPECT_EQ(cluster.common.rawSpanRef, (RawSpan{}));
    EXPECT_EQ(cluster.baudrate, std::nullopt);
    EXPECT_TRUE(cluster.physicalChannels.empty());
}

TEST(ClusterTest, StoresPayloadData)
{
    Cluster cluster{};
    cluster.common.shortName = "cluster_a";
    cluster.common.category = "powertrain";
    cluster.common.rawSpanRef = RawSpan{10U, 20U, 3U};
    cluster.baudrate = 500000U;
    cluster.physicalChannels = {"can0", "can1"};

    EXPECT_EQ(cluster.common.shortName, "cluster_a");
    ASSERT_TRUE(cluster.common.category.has_value());
    EXPECT_EQ(*cluster.common.category, "powertrain");
    EXPECT_EQ(cluster.common.rawSpanRef, (RawSpan{10U, 20U, 3U}));
    ASSERT_TRUE(cluster.baudrate.has_value());
    EXPECT_EQ(*cluster.baudrate, 500000U);
    ASSERT_EQ(cluster.physicalChannels.size(), 2U);
    EXPECT_EQ(cluster.physicalChannels[0], "can0");
    EXPECT_EQ(cluster.physicalChannels[1], "can1");
}
