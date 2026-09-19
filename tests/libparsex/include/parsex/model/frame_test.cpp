#include <gtest/gtest.h>
#include <parsex/model/frame.hpp>
#include <parsex/raw/raw_span.hpp>

TEST(FrameTest, DefaultConstructionInitializesEmptyFields)
{
    Frame frame{};

    EXPECT_EQ(frame.common.shortName, "");
    EXPECT_EQ(frame.common.category, std::nullopt);
    EXPECT_EQ(frame.common.rawSpanRef, (RawSpan{}));
    EXPECT_EQ(frame.length, 0U);
    EXPECT_TRUE(frame.transmitters.empty());
    EXPECT_TRUE(frame.pdus.empty());
}

TEST(FrameTest, StoresPayloadData)
{
    Frame frame{};
    frame.common.shortName = "frame_a";
    frame.common.category = "standard";
    frame.common.rawSpanRef = RawSpan{1U, 8U, 2U};
    frame.length = 8U;
    frame.transmitters = {"ecu1", "ecu2"};
    frame.pdus = {
        FramePduMapping{"pdu_a", 0U},
        FramePduMapping{"pdu_b", 32U}
    };

    EXPECT_EQ(frame.common.shortName, "frame_a");
    ASSERT_TRUE(frame.common.category.has_value());
    EXPECT_EQ(*frame.common.category, "standard");
    EXPECT_EQ(frame.common.rawSpanRef, (RawSpan{1U, 8U, 2U}));
    EXPECT_EQ(frame.length, 8U);
    ASSERT_EQ(frame.transmitters.size(), 2U);
    EXPECT_EQ(frame.transmitters[0], "ecu1");
    EXPECT_EQ(frame.transmitters[1], "ecu2");
    ASSERT_EQ(frame.pdus.size(), 2U);
    EXPECT_EQ(frame.pdus[0].pduShortNameRef, "pdu_a");
    EXPECT_EQ(frame.pdus[0].startPosition, 0U);
    EXPECT_EQ(frame.pdus[1].pduShortNameRef, "pdu_b");
    EXPECT_EQ(frame.pdus[1].startPosition, 32U);
}