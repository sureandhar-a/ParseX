#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <parsex/export/can_db.hpp>
#include <parsex/parser/parser.hpp>

namespace {

const std::string kFixtureDir = PARSEX_FIXTURE_DIR;

const ExportFrame* findFrame(const ExportDatabase& db, const std::string& name) {
    const auto it = std::ranges::find(db.frames, name, &ExportFrame::name);
    return it == db.frames.end() ? nullptr : &*it;
}

const ExportSignal* findSignal(const ExportFrame& frame, const std::string& name) {
    const auto it = std::ranges::find(frame.signals, name, &ExportSignal::name);
    return it == frame.signals.end() ? nullptr : &*it;
}

bool hasWarning(const ExportDatabase& db, const std::string& needle) {
    return std::ranges::any_of(db.warnings, [&](const Warning& warning) {
        return warning.message.find(needle) != std::string::npos;
    });
}

// Expected values below were taken from cantools 44 loading the same file.
class SystemFourTwo : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        const ParsedFile file = Parser{}.parseFile(kFixtureDir + "/system-4.2.arxml");
        db_ = new ExportDatabase(buildExportDatabase(file));
    }
    static void TearDownTestSuite() {
        delete db_;
        db_ = nullptr;
    }
    static ExportDatabase* db_;
};
ExportDatabase* SystemFourTwo::db_ = nullptr;

}  // namespace

TEST_F(SystemFourTwo, BusNodesAndFrameOrder) {
    EXPECT_EQ(db_->busName, "Cluster0");
    EXPECT_EQ(db_->baudrate, 500000U);
    EXPECT_EQ(db_->nodes, (std::vector<std::string>{"DJ", "Dancer", "Guard"}));
    std::vector<std::string> names;
    for (const ExportFrame& frame : db_->frames) {
        names.push_back(frame.name);
    }
    EXPECT_EQ(names, (std::vector<std::string>{"MultiplexedMessage", "Message1", "Message2", "Message3",
                                               "Message4", "OneToContainThemAll", "AlarmStatus",
                                               "MessageWithoutPDU"}));
}

TEST_F(SystemFourTwo, FrameIdsExtendedFdAndLength) {
    struct Expect {
        const char* name;
        std::uint32_t id;
        bool extended;
        bool fd;
        std::uint32_t length;
    };
    const Expect expected[] = {
        {"MultiplexedMessage", 4, false, true, 2}, {"Message1", 5, false, true, 9},
        {"Message2", 6, true, true, 7},            {"Message3", 100, false, false, 6},
        {"Message4", 101, false, false, 6},        {"OneToContainThemAll", 102, false, false, 64},
        {"AlarmStatus", 1001, false, false, 1},    {"MessageWithoutPDU", 1002, false, false, 8},
    };
    for (const Expect& want : expected) {
        const ExportFrame* frame = findFrame(*db_, want.name);
        ASSERT_NE(frame, nullptr) << want.name;
        EXPECT_EQ(frame->id, want.id) << want.name;
        EXPECT_EQ(frame->extended, want.extended) << want.name;
        EXPECT_EQ(frame->canFd, want.fd) << want.name;
        EXPECT_EQ(frame->length, want.length) << want.name;
    }
}

TEST_F(SystemFourTwo, LittleEndianPlainSignals) {
    const ExportFrame* frame = findFrame(*db_, "Message1");
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->signals.size(), 5U);
    const ExportSignal* counter = findSignal(*frame, "message1_SeqCounter");
    ASSERT_NE(counter, nullptr);
    EXPECT_EQ(counter->startBit, 0U);
    EXPECT_EQ(counter->bitLength, 16U);
    EXPECT_EQ(counter->byteOrder, ByteOrder::LeastSignificantByteFirst);
    EXPECT_FALSE(counter->isSigned);
    EXPECT_DOUBLE_EQ(counter->factor, 1.0);
    EXPECT_DOUBLE_EQ(counter->offset, 0.0);
    EXPECT_FALSE(counter->min.has_value());
    EXPECT_EQ(counter->unit, "");
    const ExportSignal* wide = findSignal(*frame, "signal5");
    ASSERT_NE(wide, nullptr);
    EXPECT_EQ(wide->startBit, 40U);
    EXPECT_EQ(wide->bitLength, 32U);
}

TEST_F(SystemFourTwo, BigEndianScaledSignalWithUnitAndLimits) {
    const ExportFrame* frame = findFrame(*db_, "Message1");
    ASSERT_NE(frame, nullptr);
    const ExportSignal* signal = findSignal(*frame, "signal1");
    ASSERT_NE(signal, nullptr);
    EXPECT_EQ(signal->startBit, 36U);
    EXPECT_EQ(signal->bitLength, 3U);
    EXPECT_EQ(signal->byteOrder, ByteOrder::MostSignificantByteFirst);
    EXPECT_FALSE(signal->isSigned);
    // (0 + 1e1 * x) / 2 -> factor 5; LOWER/UPPER 0..4 -> 0..20.
    EXPECT_DOUBLE_EQ(signal->factor, 5.0);
    EXPECT_DOUBLE_EQ(signal->offset, 0.0);
    EXPECT_EQ(signal->min, 0.0);
    EXPECT_EQ(signal->max, 20.0);
    EXPECT_EQ(signal->unit, "m");
}

TEST_F(SystemFourTwo, MixedLinearAndTextTableMethod) {
    const ExportFrame* frame = findFrame(*db_, "Message1");
    ASSERT_NE(frame, nullptr);
    const ExportSignal* signal = findSignal(*frame, "signal6");
    ASSERT_NE(signal, nullptr);
    EXPECT_EQ(signal->startBit, 32U);
    EXPECT_EQ(signal->bitLength, 1U);
    EXPECT_DOUBLE_EQ(signal->factor, 0.1);
    EXPECT_EQ(signal->min, 0.0);
    EXPECT_DOUBLE_EQ(signal->max.value_or(-1), 0.1);
    EXPECT_EQ(signal->unit, "wp");
    ASSERT_EQ(signal->valueTable.size(), 1U);
    EXPECT_EQ(signal->valueTable[0].value, 0);
    EXPECT_EQ(signal->valueTable[0].label, "zero");
}

TEST_F(SystemFourTwo, ValueTableAndNoUnitAndSigned) {
    const ExportFrame* frame = findFrame(*db_, "Message2");
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->signals.size(), 3U);
    const ExportSignal* choices = findSignal(*frame, "signal4");
    ASSERT_NE(choices, nullptr);
    EXPECT_EQ(choices->startBit, 30U);
    EXPECT_EQ(choices->bitLength, 4U);
    EXPECT_EQ(choices->unit, "");  // /Unit/zilch is "NoUnit"
    ASSERT_EQ(choices->valueTable.size(), 2U);
    EXPECT_EQ(choices->valueTable[0].value, 1);
    EXPECT_EQ(choices->valueTable[0].label, "one");
    EXPECT_EQ(choices->valueTable[1].value, 2);
    EXPECT_EQ(choices->valueTable[1].label, "two");
    const ExportSignal* negative = findSignal(*frame, "signal2");
    ASSERT_NE(negative, nullptr);
    EXPECT_TRUE(negative->isSigned);
    EXPECT_EQ(negative->startBit, 18U);
    EXPECT_EQ(negative->bitLength, 11U);
    EXPECT_EQ(negative->byteOrder, ByteOrder::LeastSignificantByteFirst);
    const ExportSignal* small = findSignal(*frame, "signal3");
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->startBit, 6U);
    EXPECT_EQ(small->bitLength, 2U);
}

TEST_F(SystemFourTwo, TransmittersAndReceiversFromPduGroups) {
    const ExportFrame* message1 = findFrame(*db_, "Message1");
    const ExportFrame* message2 = findFrame(*db_, "Message2");
    ASSERT_NE(message1, nullptr);
    ASSERT_NE(message2, nullptr);
    // RX/<ecu> and TX/<ecu> groups share short names; direction must not mix.
    EXPECT_EQ(message1->transmitters, (std::vector<std::string>{"DJ"}));
    for (const ExportSignal& signal : message1->signals) {
        EXPECT_EQ(signal.receivers, (std::vector<std::string>{"Dancer"})) << signal.name;
    }
    EXPECT_EQ(message2->transmitters, (std::vector<std::string>{"Dancer"}));
    for (const ExportSignal& signal : message2->signals) {
        EXPECT_EQ(signal.receivers, (std::vector<std::string>{"DJ"})) << signal.name;
    }
}

TEST_F(SystemFourTwo, UnsupportedPdusWarnAndFramesKeepZeroSignals) {
    const ExportFrame* secured = findFrame(*db_, "Message3");
    ASSERT_NE(secured, nullptr);
    EXPECT_TRUE(secured->signals.empty());
    EXPECT_TRUE(hasWarning(*db_, "Frame 'Message3': PDU 'message3_secured' is not exported"));
    EXPECT_TRUE(hasWarning(*db_, "Frame 'MultiplexedMessage': PDU 'multiplexed_message' is not exported"));
    EXPECT_TRUE(hasWarning(*db_, "Frame 'OneToContainThemAll'"));
    EXPECT_TRUE(hasWarning(*db_, "Frame 'AlarmStatus'"));
    const ExportFrame* bare = findFrame(*db_, "MessageWithoutPDU");
    ASSERT_NE(bare, nullptr);
    EXPECT_TRUE(bare->signals.empty());
}

TEST(CanDbStudyVocabulary, FrameWithoutTriggeringIsSkippedWithWarning) {
    const ParsedFile file = Parser{}.parseFile(kFixtureDir + "/parsefile_complete.arxml");
    const ExportDatabase db = buildExportDatabase(file);
    EXPECT_TRUE(db.frames.empty());
    EXPECT_TRUE(hasWarning(db, "Frame 'Frame_1' skipped"));
    EXPECT_EQ(db.busName, "CAN_Cluster");
    EXPECT_EQ(db.baudrate, 500000U);
    EXPECT_EQ(db.nodes, (std::vector<std::string>{"ECU_A"}));
}

TEST(CanDbStudyVocabulary, ModelValuesFillInWhenNoRawScalingResolves) {
    std::ifstream in(kFixtureDir + "/parsefile_complete.arxml");
    std::string xml((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string trigger =
        "<CAN-FRAME-TRIGGERING><SHORT-NAME>t</SHORT-NAME><FRAME-REF DEST=\"FRAME\">/Sys/Frame_1</FRAME-REF>"
        "<CAN-ADDRESSING-MODE>EXTENDED</CAN-ADDRESSING-MODE><IDENTIFIER>0x1A</IDENTIFIER>"
        "</CAN-FRAME-TRIGGERING></ELEMENTS>";
    xml.replace(xml.rfind("</ELEMENTS>"), std::string("</ELEMENTS>").size(), trigger);
    const std::vector<std::uint8_t> bytes(xml.begin(), xml.end());
    ParsedFile file = Parser{}.parseBytes(bytes);
    file.signals.front().factor = 0.5;
    file.signals.front().offset = -3.0;
    file.signals.front().unit = "rpm";
    file.signals.front().valueTable = std::vector<ValueTableEntry>{{1, "On"}};

    const ExportDatabase db = buildExportDatabase(file);
    ASSERT_EQ(db.frames.size(), 1U);
    const ExportFrame& frame = db.frames.front();
    EXPECT_EQ(frame.id, 0x1AU);  // hex IDENTIFIER
    EXPECT_TRUE(frame.extended);
    EXPECT_FALSE(frame.canFd);
    EXPECT_EQ(frame.length, 8U);
    EXPECT_EQ(frame.transmitters, (std::vector<std::string>{"ECU_A"}));  // Frame.transmitters
    ASSERT_EQ(frame.signals.size(), 1U);
    const ExportSignal& signal = frame.signals.front();
    EXPECT_EQ(signal.name, "Signal_1");
    EXPECT_EQ(signal.startBit, 0U);
    EXPECT_EQ(signal.bitLength, 16U);
    EXPECT_EQ(signal.byteOrder, ByteOrder::MostSignificantByteFirst);
    EXPECT_DOUBLE_EQ(signal.factor, 0.5);
    EXPECT_DOUBLE_EQ(signal.offset, -3.0);
    EXPECT_EQ(signal.unit, "rpm");
    ASSERT_EQ(signal.valueTable.size(), 1U);
    EXPECT_EQ(signal.valueTable[0].label, "On");
    EXPECT_EQ(signal.receivers, (std::vector<std::string>{"ECU_A"}));  // Signal.receivers
}

TEST(CanDbNoRawDocument, SkipsEveryFrameWithoutCrashing) {
    ParsedFile file = Parser{}.parseFile(kFixtureDir + "/system-4.2.arxml");
    file.rawDocument.reset();
    const ExportDatabase db = buildExportDatabase(file);
    EXPECT_TRUE(db.frames.empty());
    EXPECT_EQ(db.nodes.size(), 3U);
    EXPECT_EQ(db.busName, "Cluster0");
    EXPECT_TRUE(hasWarning(db, "Frame 'Message1' skipped"));
}

TEST(CanDbNoRawDocument, EmptyFileYieldsEmptyDatabase) {
    const ExportDatabase db = buildExportDatabase(ParsedFile{});
    EXPECT_TRUE(db.frames.empty());
    EXPECT_TRUE(db.warnings.empty());
    EXPECT_EQ(db.busName, "");
    EXPECT_FALSE(db.baudrate.has_value());
}

TEST_F(SystemFourTwo, SignedSignalsAndBinaryLiteralInitValue) {
    const ExportFrame* frame = findFrame(*db_, "Message4");
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->signals.size(), 3U);
    const std::uint32_t starts[] = {0, 16, 32};
    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_TRUE(frame->signals[i].isSigned) << frame->signals[i].name;
        EXPECT_EQ(frame->signals[i].startBit, starts[i]);
        EXPECT_EQ(frame->signals[i].bitLength, 11U);
        EXPECT_EQ(frame->signals[i].byteOrder, ByteOrder::LeastSignificantByteFirst);
    }
    EXPECT_EQ(frame->transmitters, (std::vector<std::string>{"DJ"}));
    const ExportFrame* message1 = findFrame(*db_, "Message1");
    ASSERT_NE(message1, nullptr);
    EXPECT_EQ(findSignal(*message1, "signal1")->initValue, 5.0);  // VALUE 0b101
}
