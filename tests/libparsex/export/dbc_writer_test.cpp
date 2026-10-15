#include <gtest/gtest.h>

#include <string>

#include <parsex/export/dbc_writer.hpp>

namespace {

bool has(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

ExportSignal makeSignal(const std::string& name) {
    ExportSignal signal;
    signal.name = name;
    signal.bitLength = 8;
    return signal;
}

ExportDatabase oneFrame(ExportFrame frame) {
    ExportDatabase database;
    database.frames.push_back(std::move(frame));
    return database;
}

ExportFrame makeFrame(const std::string& name, std::uint32_t id) {
    ExportFrame frame;
    frame.name = name;
    frame.id = id;
    frame.length = 8;
    return frame;
}

}  // namespace

TEST(DbcWriterTest, EmptyDatabaseHasHeaderSectionsOnly) {
    const std::string dbc = toDbc({});
    EXPECT_EQ(dbc.rfind("VERSION \"\"\n\nNS_ :\n\tNS_DESC_\n\tCM_\n", 0), 0U);
    EXPECT_TRUE(has(dbc, "\tSG_MUL_VAL_\n\nBS_:\n\nBU_:\n\n"));
    EXPECT_FALSE(has(dbc, "BO_ "));
    EXPECT_FALSE(has(dbc, "BA_DEF_ BO_"));
    EXPECT_EQ(dbc.back(), '\n');
}

TEST(DbcWriterTest, SmallGoldenFile) {
    ExportFrame frame = makeFrame("Engine", 0x100);
    frame.transmitters = {"Ecu1"};
    ExportSignal rpm = makeSignal("Rpm");
    rpm.startBit = 8;
    rpm.bitLength = 16;
    rpm.factor = 0.25;
    rpm.min = 0.0;
    rpm.max = 8000.0;
    rpm.unit = "rpm";
    rpm.receivers = {"Ecu2", "Ecu3"};
    frame.signals.push_back(rpm);
    ExportDatabase database = oneFrame(frame);
    database.nodes = {"Ecu1", "Ecu2", "Ecu3"};

    const std::string dbc = toDbc(database);
    const auto bs = dbc.find("BS_:");
    ASSERT_NE(bs, std::string::npos);
    EXPECT_EQ(dbc.substr(bs),
              "BS_:\n\nBU_: Ecu1 Ecu2 Ecu3\n\n"
              "BO_ 256 Engine: 8 Ecu1\n"
              " SG_ Rpm : 8|16@1+ (0.25,0) [0|8000] \"rpm\" Ecu2,Ecu3\n\n");
}

TEST(DbcWriterTest, ExtendedIdCarriesFlagBit) {
    ExportFrame frame = makeFrame("Ext", 0x1234);
    frame.extended = true;
    const std::string dbc = toDbc(oneFrame(frame));
    EXPECT_TRUE(has(dbc, "BO_ " + std::to_string(0x80001234U) + " Ext: 8 Vector__XXX\n"));
}

TEST(DbcWriterTest, ByteOrderAndSignedness) {
    ExportFrame frame = makeFrame("F", 1);
    ExportSignal big = makeSignal("Big");
    big.byteOrder = ByteOrder::MostSignificantByteFirst;
    big.isSigned = true;
    big.startBit = 7;
    frame.signals.push_back(big);
    ExportSignal little = makeSignal("Little");
    frame.signals.push_back(little);
    const std::string dbc = toDbc(oneFrame(frame));
    EXPECT_TRUE(has(dbc, " SG_ Big : 7|8@0- (1,0) [0|0] \"\" Vector__XXX\n"));
    EXPECT_TRUE(has(dbc, " SG_ Little : 0|8@1+ (1,0) [0|0] \"\" Vector__XXX\n"));
}

TEST(DbcWriterTest, NumbersAreShortestRoundTrip) {
    ExportFrame frame = makeFrame("F", 1);
    ExportSignal signal = makeSignal("S");
    signal.factor = 0.1;
    signal.offset = -40.5;
    frame.signals.push_back(signal);
    EXPECT_TRUE(has(toDbc(oneFrame(frame)), "(0.1,-40.5) [0|0]"));
}

TEST(DbcWriterTest, OnlyOneLimitPresentPrintsZeroForTheOther) {
    ExportFrame frame = makeFrame("F", 1);
    ExportSignal onlyMax = makeSignal("A");
    onlyMax.max = 100.0;
    ExportSignal onlyMin = makeSignal("B");
    onlyMin.min = -5.0;
    frame.signals = {onlyMax, onlyMin};
    const std::string dbc = toDbc(oneFrame(frame));
    EXPECT_TRUE(has(dbc, "[0|100]"));
    EXPECT_TRUE(has(dbc, "[-5|0]"));
}

TEST(DbcWriterTest, UnitIsEscaped) {
    ExportFrame frame = makeFrame("F", 1);
    ExportSignal signal = makeSignal("S");
    signal.unit = "a\"b\\c";
    frame.signals.push_back(signal);
    EXPECT_TRUE(has(toDbc(oneFrame(frame)), "\"a\\\"b\\\\c\""));
}

TEST(DbcWriterTest, MultipleTransmittersEmitBoTxBu) {
    ExportFrame frame = makeFrame("F", 0x10);
    frame.transmitters = {"A", "B"};
    const std::string dbc = toDbc(oneFrame(frame));
    EXPECT_TRUE(has(dbc, "BO_ 16 F: 8 A\n"));
    EXPECT_TRUE(has(dbc, "BO_TX_BU_ 16 : A,B;\n"));
}

TEST(DbcWriterTest, NoFdFramesMeansNoAttributeSection) {
    ExportFrame frame = makeFrame("F", 1);
    frame.extended = true;
    const std::string dbc = toDbc(oneFrame(frame));
    EXPECT_FALSE(has(dbc, "VFrameFormat"));
    EXPECT_FALSE(has(dbc, "BusType"));
}

TEST(DbcWriterTest, CanFdEmitsAttributesForNonDefaultFramesOnly) {
    ExportFrame classic = makeFrame("Classic", 1);
    ExportFrame classicExt = makeFrame("ClassicExt", 2);
    classicExt.extended = true;
    ExportFrame fd = makeFrame("Fd", 3);
    fd.canFd = true;
    ExportFrame fdExt = makeFrame("FdExt", 4);
    fdExt.canFd = true;
    fdExt.extended = true;
    ExportDatabase database;
    database.frames = {classic, classicExt, fd, fdExt};

    const std::string dbc = toDbc(database);
    EXPECT_TRUE(has(dbc, "BA_DEF_ BO_ \"VFrameFormat\" ENUM \"StandardCAN\",\"ExtendedCAN\",\"reserved\""));
    EXPECT_TRUE(has(dbc, ",\"StandardCAN_FD\",\"ExtendedCAN_FD\";\n"));
    EXPECT_TRUE(has(dbc, "BA_DEF_ \"BusType\" STRING ;\n"));
    EXPECT_TRUE(has(dbc, "BA_DEF_DEF_ \"VFrameFormat\" \"StandardCAN\";\n"));
    EXPECT_TRUE(has(dbc, "BA_DEF_DEF_ \"BusType\" \"CAN\";\n"));
    EXPECT_TRUE(has(dbc, "BA_ \"BusType\" \"CAN FD\";\n"));
    EXPECT_FALSE(has(dbc, "BA_ \"VFrameFormat\" BO_ 1 "));
    EXPECT_TRUE(has(dbc, "BA_ \"VFrameFormat\" BO_ " + std::to_string(0x80000002U) + " 1;\n"));
    EXPECT_TRUE(has(dbc, "BA_ \"VFrameFormat\" BO_ 3 14;\n"));
    EXPECT_TRUE(has(dbc, "BA_ \"VFrameFormat\" BO_ " + std::to_string(0x80000004U) + " 15;\n"));
}

TEST(DbcWriterTest, ValueTablesBecomeValLines) {
    ExportFrame frame = makeFrame("F", 5);
    ExportSignal gear = makeSignal("Gear");
    gear.valueTable = {{0, "Park"}, {1, "Say \"R\""}};
    frame.signals = {gear, makeSignal("Plain")};
    const std::string dbc = toDbc(oneFrame(frame));
    EXPECT_TRUE(has(dbc, "VAL_ 5 Gear 0 \"Park\" 1 \"Say \\\"R\\\"\" ;\n"));
    EXPECT_FALSE(has(dbc, "VAL_ 5 Plain"));
}

TEST(DbcWriterTest, NamesAreSanitised) {
    ExportFrame frame = makeFrame("1 bad-name", 1);
    frame.transmitters = {"ecu.1"};
    ExportSignal signal = makeSignal("");
    signal.receivers = {"rx/2"};
    frame.signals.push_back(signal);
    ExportDatabase database = oneFrame(frame);
    database.nodes = {"ecu.1"};
    const std::string dbc = toDbc(database);
    EXPECT_TRUE(has(dbc, "BU_: ecu_1\n"));
    EXPECT_TRUE(has(dbc, "BO_ 1 _1_bad_name: 8 ecu_1\n"));
    EXPECT_TRUE(has(dbc, " SG_ _ : 0|8@1+"));
    EXPECT_TRUE(has(dbc, "\"\" rx_2\n"));
}

TEST(DbcWriterTest, OutputIsDeterministic) {
    ExportFrame frame = makeFrame("F", 1);
    frame.canFd = true;
    ExportSignal signal = makeSignal("S");
    signal.factor = 0.1;
    signal.valueTable = {{1, "One"}};
    frame.signals.push_back(signal);
    const ExportDatabase database = oneFrame(frame);
    EXPECT_EQ(toDbc(database), toDbc(database));
}
