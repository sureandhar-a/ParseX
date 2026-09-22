// Unit tests for key-based nested-collection diffing (PAR-128).
#include <gtest/gtest.h>

#include <parsex/diff/diff_nested.hpp>
#include <parsex/model/signal_group.hpp>

TEST(NestedCollectionTest, ReorderedMembersYieldZeroDiffs) {
    SignalGroup oldG;
    oldG.common.shortName = "G";
    oldG.members = {"S1", "S2", "S3"};

    SignalGroup newG = oldG;
    newG.members = {"S3", "S1", "S2"};

    EXPECT_TRUE(diffSignalGroupElements(oldG, newG).empty());
}

TEST(NestedCollectionTest, AddedMemberYieldsOneKeyedEntry) {
    SignalGroup oldG;
    oldG.common.shortName = "G";
    oldG.members = {"S1"};

    SignalGroup newG = oldG;
    newG.members = {"S1", "S2"};

    const std::vector<FieldDiff> diffs = diffSignalGroupElements(oldG, newG);
    ASSERT_EQ(diffs.size(), 1U);
    EXPECT_EQ(diffs[0].fieldName, "members[S2]");
    EXPECT_EQ(diffs[0].oldValue, "absent");
    EXPECT_EQ(diffs[0].newValue, "S2");
}

TEST(NestedCollectionTest, ValueTableChangeKeyedToNumericCode) {
    Signal oldS;
    oldS.common.shortName = "S";
    oldS.valueTable = std::vector<ValueTableEntry>{{.value = 0, .label = "Off"},
                                                   {.value = 1, .label = "On"}};

    Signal newS = oldS;
    newS.valueTable->at(1).label = "Enabled";

    const std::vector<FieldDiff> diffs = diffSignalElements(oldS, newS);
    ASSERT_EQ(diffs.size(), 1U);
    EXPECT_EQ(diffs[0].fieldName, "valueTable[1].label");
    EXPECT_EQ(diffs[0].oldValue, "On");
    EXPECT_EQ(diffs[0].newValue, "Enabled");
}
