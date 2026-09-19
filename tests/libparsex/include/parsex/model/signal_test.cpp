#include <gtest/gtest.h>
#include <parsex/model/signal.hpp>

TEST(SignalTest, DefaultConstructionHasIdentityFactorOffset)
{
    Signal sig{};

    EXPECT_DOUBLE_EQ(sig.factor, 1.0);
    EXPECT_DOUBLE_EQ(sig.offset, 0.0);
    EXPECT_EQ(sig.startBit, 0U);
    EXPECT_EQ(sig.bitLength, 0U);
    EXPECT_EQ(sig.byteOrder, ByteOrder::MostSignificantByteFirst);
    EXPECT_FALSE(sig.isSigned);
    EXPECT_FALSE(sig.min.has_value());
    EXPECT_FALSE(sig.max.has_value());
    EXPECT_FALSE(sig.unit.has_value());
    EXPECT_FALSE(sig.initValue.has_value());
    EXPECT_TRUE(sig.receivers.empty());
    EXPECT_FALSE(sig.valueTable.has_value());
}