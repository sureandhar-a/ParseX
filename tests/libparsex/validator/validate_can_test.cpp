// PAR-108: DLC/payload-length validation for classic CAN and CAN FD.
#include <gtest/gtest.h>

#include <parsex/validator/validator.hpp>

TEST(CanDlcTest, ClassicLengthsZeroToEightPassFdTableIsExact) {
    for (std::uint32_t len = 0; len <= 8; ++len) {
        EXPECT_TRUE(Validator::isValidClassicCanLength(len));
        EXPECT_TRUE(Validator::isValidCanFdLength(len));
    }
    EXPECT_FALSE(Validator::isValidClassicCanLength(9));
    // Non-linear FD lengths.
    for (std::uint32_t len : {12U, 16U, 20U, 24U, 32U, 48U, 64U}) {
        EXPECT_TRUE(Validator::isValidCanFdLength(len));
    }
    for (std::uint32_t len : {9U, 10U, 11U, 13U, 40U, 65U}) {
        EXPECT_FALSE(Validator::isValidCanFdLength(len));
    }
}

TEST(CanDlcTest, AllSixteenDlcValuesMapCorrectly) {
    const std::uint32_t expected[16] = {0, 1, 2, 3,  4,  5,  6,  7,
                                        8, 12, 16, 20, 24, 32, 48, 64};
    for (int dlc = 0; dlc < 16; ++dlc) {
        const auto bytes = Validator::canFdBytesForDlc(dlc);
        ASSERT_TRUE(bytes.has_value());
        EXPECT_EQ(bytes.value(), expected[dlc]) << "DLC " << dlc;
    }
    EXPECT_FALSE(Validator::canFdBytesForDlc(-1).has_value());
    EXPECT_FALSE(Validator::canFdBytesForDlc(16).has_value());
}

TEST(CanDlcTest, FrameAndPduLengthsValidated) {
    ParsedFile file;
    Frame goodFrame;
    goodFrame.common.shortName = "Good";
    goodFrame.length = 8;
    file.frames.push_back(goodFrame);
    Pdu goodPdu;
    goodPdu.common.shortName = "PduGood";
    goodPdu.length = 16;  // valid CAN FD length
    file.pdus.push_back(goodPdu);
    ParsedProject goodProject;
    goodProject.files.push_back(file);
    EXPECT_TRUE(Validator{}.validateCanSemantics(goodProject).errors.empty());

    ParsedFile badFile;
    Frame badFrame;
    badFrame.common.shortName = "Bad";
    badFrame.length = 9;  // invalid in both
    badFile.frames.push_back(badFrame);
    Pdu badPdu;
    badPdu.common.shortName = "PduBad";
    badPdu.length = 40;  // not in FD set
    badFile.pdus.push_back(badPdu);
    ParsedProject badProject;
    badProject.files.push_back(badFile);
    const ValidationResult result = Validator{}.validateCanSemantics(badProject);
    ASSERT_EQ(result.errors.size(), 2U);
    for (const auto& err : result.errors) {
        EXPECT_EQ(err.code, "can.dlc_mismatch");
    }
}
