#include <gtest/gtest.h>
#include <parsex/model/pdu.hpp>
#include <parsex/raw/raw_span.hpp>

TEST(PduTest, DefaultConstructionInitializesEmptyFields)
{
    Pdu pdu{};

    EXPECT_EQ(pdu.common.shortName, "");
    EXPECT_EQ(pdu.common.category, std::nullopt);
    EXPECT_EQ(pdu.common.rawSpanRef, (RawSpan{}));
    EXPECT_EQ(pdu.length, 0U);
    EXPECT_TRUE(pdu.signalMappings.empty());
}

TEST(PduTest, StoresPayloadData)
{
    Pdu pdu{};
    pdu.common.shortName = "pdu_a";
    pdu.common.category = "standard";
    pdu.common.rawSpanRef = RawSpan{1U, 8U, 2U};
    pdu.length = 8U;
    pdu.signalMappings = {
        PduSignalMapping{"signal_a", 0U, ByteOrder::MostSignificantByteFirst},
        PduSignalMapping{"signal_b", 16U, ByteOrder::LeastSignificantByteFirst}
    };

    EXPECT_EQ(pdu.common.shortName, "pdu_a");
    ASSERT_TRUE(pdu.common.category.has_value());
    EXPECT_EQ(*pdu.common.category, "standard");
    EXPECT_EQ(pdu.common.rawSpanRef, (RawSpan{1U, 8U, 2U}));
    EXPECT_EQ(pdu.length, 8U);
    ASSERT_EQ(pdu.signalMappings.size(), 2U);
    EXPECT_EQ(pdu.signalMappings[0].signalShortNameRef, "signal_a");
    EXPECT_EQ(pdu.signalMappings[0].startPosition, 0U);
    EXPECT_EQ(pdu.signalMappings[0].byteOrder, ByteOrder::MostSignificantByteFirst);
    EXPECT_EQ(pdu.signalMappings[1].signalShortNameRef, "signal_b");
    EXPECT_EQ(pdu.signalMappings[1].startPosition, 16U);
    EXPECT_EQ(pdu.signalMappings[1].byteOrder, ByteOrder::LeastSignificantByteFirst);
}