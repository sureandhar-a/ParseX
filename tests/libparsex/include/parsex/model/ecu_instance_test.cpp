#include <gtest/gtest.h>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/raw/raw_span.hpp>

TEST(EcuInstanceTest, DefaultConstructionInitializesEmptyFields)
{
    EcuInstance ecuInstance{};

    EXPECT_EQ(ecuInstance.common.shortName, "");
    EXPECT_EQ(ecuInstance.common.category, std::nullopt);
    EXPECT_EQ(ecuInstance.common.rawSpanRef, (RawSpan{}));
    EXPECT_TRUE(ecuInstance.connectedChannels.empty());
    EXPECT_TRUE(ecuInstance.controllers.empty());
}

TEST(EcuInstanceTest, StoresPayloadData)
{
    EcuInstance ecuInstance{};
    ecuInstance.common.shortName = "ecu_a";
    ecuInstance.common.category = "body";
    ecuInstance.common.rawSpanRef = RawSpan{1U, 8U, 2U};
    ecuInstance.connectedChannels = {"eth0", "can2"};
    ecuInstance.controllers = {"controller_alpha", "controller_beta"};

    EXPECT_EQ(ecuInstance.common.shortName, "ecu_a");
    ASSERT_TRUE(ecuInstance.common.category.has_value());
    EXPECT_EQ(*ecuInstance.common.category, "body");
    EXPECT_EQ(ecuInstance.common.rawSpanRef, (RawSpan{1U, 8U, 2U}));
    ASSERT_EQ(ecuInstance.connectedChannels.size(), 2U);
    EXPECT_EQ(ecuInstance.connectedChannels[0], "eth0");
    EXPECT_EQ(ecuInstance.connectedChannels[1], "can2");
    ASSERT_EQ(ecuInstance.controllers.size(), 2U);
    EXPECT_EQ(ecuInstance.controllers[0], "controller_alpha");
    EXPECT_EQ(ecuInstance.controllers[1], "controller_beta");
}