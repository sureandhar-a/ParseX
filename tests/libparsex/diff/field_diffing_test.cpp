// Consolidated field-level diffing coverage for PAR-118 Done-when criteria
// (PAR-130): flat-field change, reordered nested collection, added nested
// member, and value-table entry change.
#include <gtest/gtest.h>

#include <parsex/diff/diff_fields.hpp>
#include <parsex/diff/diff_nested.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>

TEST(FieldDiffingTest, SignalStartBitChangeIsSingleFieldDiff) {
    Signal oldSig;
    oldSig.common.shortName = "S";
    oldSig.startBit = 3;
    oldSig.bitLength = 8;

    Signal newSig = oldSig;
    newSig.startBit = 11;

    const std::vector<FieldDiff> diffs = diffStruct(oldSig, newSig);
    ASSERT_EQ(diffs.size(), 1U);
    EXPECT_EQ(diffs[0].fieldName, "startBit");
    EXPECT_EQ(diffs[0].oldValue, "3");
    EXPECT_EQ(diffs[0].newValue, "11");
}

TEST(FieldDiffingTest, ReorderedMemberListYieldsZeroDiffs) {
    SignalGroup oldG;
    oldG.common.shortName = "G";
    oldG.members = {"S1", "S2"};

    SignalGroup newG = oldG;
    newG.members = {"S2", "S1"};

    EXPECT_TRUE(diffSignalGroupElements(oldG, newG).empty());
}

TEST(FieldDiffingTest, AddedMemberYieldsOneNestedFinding) {
    SignalGroup oldG;
    oldG.common.shortName = "G";
    oldG.members = {"S1"};

    SignalGroup newG = oldG;
    newG.members = {"S1", "S2"};

    const std::vector<FieldDiff> diffs = diffSignalGroupElements(oldG, newG);
    ASSERT_EQ(diffs.size(), 1U);
    EXPECT_EQ(diffs[0].fieldName, "members[S2]");
    EXPECT_EQ(diffs[0].oldValue, "absent");
}

TEST(FieldDiffingTest, ValueTableChangeKeyedToNumericCode) {
    Signal oldS;
    oldS.common.shortName = "S";
    oldS.valueTable = std::vector<ValueTableEntry>{{.value = 5, .label = "Old"}};

    Signal newS = oldS;
    newS.valueTable->at(0).label = "New";

    const std::vector<FieldDiff> diffs = diffSignalElements(oldS, newS);
    ASSERT_EQ(diffs.size(), 1U);
    EXPECT_EQ(diffs[0].fieldName, "valueTable[5].label");
    EXPECT_EQ(diffs[0].oldValue, "Old");
    EXPECT_EQ(diffs[0].newValue, "New");
}
