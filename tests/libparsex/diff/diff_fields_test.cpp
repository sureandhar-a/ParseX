// Unit tests for generic diffStruct<T>() (PAR-127).
#include <gtest/gtest.h>

#include <parsex/diff/diff_fields.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/signal.hpp>

TEST(DiffStructTest, SingleStartBitChangeYieldsOneFieldDiff) {
    Signal oldSig;
    oldSig.common.shortName = "S";
    oldSig.startBit = 3;
    oldSig.bitLength = 8;

    Signal newSig = oldSig;
    newSig.startBit = 7;

    const std::vector<FieldDiff> diffs = diffStruct(oldSig, newSig);
    ASSERT_EQ(diffs.size(), 1U);
    EXPECT_EQ(diffs[0].fieldName, "startBit");
    EXPECT_EQ(diffs[0].oldValue, "3");
    EXPECT_EQ(diffs[0].newValue, "7");
}

TEST(DiffStructTest, IdenticalFramesYieldNoDiffs) {
    Frame oldFrame;
    oldFrame.common.shortName = "F";
    oldFrame.length = 8;
    oldFrame.transmitters = {"ECU_A"};

    const Frame newFrame = oldFrame;
    EXPECT_TRUE(diffStruct(oldFrame, newFrame).empty());
}

TEST(DiffStructTest, EnumChangeUsesHumanReadableNames) {
    PduSignalMapping oldMapping;
    oldMapping.signalShortNameRef = "S";
    oldMapping.byteOrder = ByteOrder::MostSignificantByteFirst;

    PduSignalMapping newMapping = oldMapping;
    newMapping.byteOrder = ByteOrder::LeastSignificantByteFirst;

    const std::vector<FieldDiff> diffs = diffStruct(oldMapping, newMapping);
    ASSERT_EQ(diffs.size(), 1U);
    EXPECT_EQ(diffs[0].fieldName, "byteOrder");
    EXPECT_EQ(diffs[0].oldValue, "MostSignificantByteFirst");
    EXPECT_EQ(diffs[0].newValue, "LeastSignificantByteFirst");
}
