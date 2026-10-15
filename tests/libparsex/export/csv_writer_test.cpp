#include <gtest/gtest.h>

#include <parsex/export/csv_writer.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kHeaderLine =
    "frame,frame_id,extended,can_fd,length,transmitters,signal,start_bit,bit_length,byte_order,signed,"
    "factor,offset,min,max,unit,init_value,receivers,value_table\r\n";

ExportDatabase smallDatabase() {
    ExportDatabase db;
    db.busName = "CAN1";

    ExportFrame engine;
    engine.name = "EngineData";
    engine.id = 0x1FF;
    engine.length = 8;
    engine.transmitters = {"Engine", "Gateway"};
    engine.signals.push_back(ExportSignal{
        .name = "EngineSpeed",
        .startBit = 0,
        .bitLength = 16,
        .byteOrder = ByteOrder::LeastSignificantByteFirst,
        .factor = 0.25,
        .offset = 0.0,
        .min = 0.0,
        .max = 8031.75,
        .unit = "rpm",
        .receivers = {"Dash", "Gateway"},
    });
    engine.signals.push_back(ExportSignal{
        .name = "Ignition",
        .startBit = 16,
        .bitLength = 1,
        .byteOrder = ByteOrder::LeastSignificantByteFirst,
        .factor = 1.0,
        .offset = 0.0,
        .initValue = 0.0,
        .receivers = {"Dash"},
        .valueTable = {{0, "Off"}, {1, "On"}},
    });
    engine.signals.push_back(ExportSignal{
        .name = "Temperature",
        .startBit = 23,
        .bitLength = 8,
        .byteOrder = ByteOrder::MostSignificantByteFirst,
        .isSigned = true,
        .factor = 1.0,
        .offset = -40.0,
        .min = -40.0,
        .max = 215.0,
        .unit = "degC",
        .receivers = {"Dash"},
    });
    db.frames.push_back(engine);

    ExportFrame temp;
    temp.name = "Temp";
    temp.id = 0x18FEF100;
    temp.extended = true;
    temp.canFd = true;
    temp.length = 64;
    temp.transmitters = {"Sensor"};
    db.frames.push_back(temp); // no signals: must still produce one row

    return db;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string::size_type start = 0;
    while (start < text.size()) {
        const auto end = text.find("\r\n", start);
        lines.push_back(text.substr(start, end - start));
        start = end + 2;
    }
    return lines;
}

}  // namespace

TEST(CsvWriterTest, EmptyDatabaseWritesHeaderOnly) {
    EXPECT_EQ(toCsv(ExportDatabase{}), std::string(kHeaderLine));
}

TEST(CsvWriterTest, GoldenOutputForSmallDatabase) {
    const std::string expected =
        std::string(kHeaderLine) +
        "EngineData,0x1FF,false,false,8,Engine;Gateway,EngineSpeed,0,16,little_endian,false,0.25,0,0,8031.75,rpm,,"
        "Dash;Gateway,\r\n"
        "EngineData,0x1FF,false,false,8,Engine;Gateway,Ignition,16,1,little_endian,false,1,0,,,,0,Dash,0=Off;1=On\r\n"
        "EngineData,0x1FF,false,false,8,Engine;Gateway,Temperature,23,8,big_endian,true,1,-40,-40,215,degC,,Dash,\r\n"
        "Temp,0x18FEF100,true,true,64,Sensor,,,,,,,,,,,,,\r\n";
    EXPECT_EQ(toCsv(smallDatabase()), expected);
}

TEST(CsvWriterTest, QuotesUnitContainingCommaAndQuote) {
    ExportDatabase db;
    ExportFrame frame;
    frame.name = "F";
    frame.id = 1;
    frame.signals.push_back(ExportSignal{.name = "S", .bitLength = 8, .unit = R"(kg, "net")"});
    db.frames.push_back(frame);

    const auto out = toCsv(db);
    EXPECT_NE(out.find(R"(,"kg, ""net""",)"), std::string::npos) << out;
}

TEST(CsvWriterTest, EveryRowEndsWithCrlfIncludingLast) {
    const auto out = toCsv(smallDatabase());
    ASSERT_GE(out.size(), 2u);
    EXPECT_EQ(out.substr(out.size() - 2), "\r\n");

    // No bare LF anywhere: every '\n' must be preceded by '\r'.
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i] == '\n') {
            ASSERT_GT(i, 0u);
            EXPECT_EQ(out[i - 1], '\r') << "bare LF at offset " << i;
        }
    }
    EXPECT_EQ(splitLines(out).size(), 5u); // header + 3 signals + 1 signal-less frame
}

TEST(CsvWriterTest, FrameIdIsUpperCaseHex) {
    ExportDatabase db;
    ExportFrame frame;
    frame.name = "F";
    frame.id = 0x1abcdef;
    frame.extended = true;
    db.frames.push_back(frame);

    const auto out = toCsv(db);
    EXPECT_NE(out.find(",0x1ABCDEF,true,"), std::string::npos) << out;
}

TEST(CsvWriterTest, BooleansAreTrueOrFalse) {
    const auto out = toCsv(smallDatabase());
    const auto lines = splitLines(out);
    ASSERT_GE(lines.size(), 5u);
    EXPECT_NE(lines[1].find(",false,false,"), std::string::npos);
    EXPECT_NE(lines[4].find(",true,true,"), std::string::npos);
    EXPECT_NE(lines[3].find(",big_endian,true,"), std::string::npos); // signed Temperature
}

TEST(CsvWriterTest, ByteOrderBigAndLittleEndian) {
    const auto lines = splitLines(toCsv(smallDatabase()));
    ASSERT_GE(lines.size(), 4u);
    EXPECT_NE(lines[1].find(",little_endian,"), std::string::npos);
    EXPECT_NE(lines[3].find(",big_endian,"), std::string::npos);
}

TEST(CsvWriterTest, ValueTableAndListsAreSemicolonJoined) {
    const auto lines = splitLines(toCsv(smallDatabase()));
    ASSERT_GE(lines.size(), 3u);
    EXPECT_TRUE(lines[2].ends_with(",0=Off;1=On")) << lines[2];
    EXPECT_NE(lines[1].find(",Engine;Gateway,"), std::string::npos);
    EXPECT_NE(lines[1].find(",Dash;Gateway,"), std::string::npos);
}

TEST(CsvWriterTest, AbsentOptionalsAndUnitAreEmptyCells) {
    ExportDatabase db;
    ExportFrame frame;
    frame.name = "F";
    frame.id = 1;
    frame.signals.push_back(ExportSignal{.name = "S", .bitLength = 8});
    db.frames.push_back(frame);

    const auto lines = splitLines(toCsv(db));
    ASSERT_EQ(lines.size(), 2u);
    // min, max, unit and init_value are all blank; receivers and value_table are blank too.
    EXPECT_EQ(lines[1], "F,0x1,false,false,0,,S,0,8,little_endian,false,1,0,,,,,,");
}

TEST(CsvWriterTest, NumbersUseShortestRoundTripForm) {
    ExportDatabase db;
    ExportFrame frame;
    frame.name = "F";
    frame.id = 1;
    frame.signals.push_back(ExportSignal{.name = "A", .bitLength = 8, .factor = 1.0, .offset = -40.0});
    frame.signals.push_back(ExportSignal{.name = "B", .bitLength = 8, .factor = 0.1, .offset = 0.0});
    db.frames.push_back(frame);

    const auto lines = splitLines(toCsv(db));
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_NE(lines[1].find(",1,-40,"), std::string::npos) << lines[1];
    EXPECT_NE(lines[2].find(",0.1,0,"), std::string::npos) << lines[2];
}

TEST(CsvWriterTest, OutputIsDeterministic) {
    const auto db = smallDatabase();
    const auto first = toCsv(db);
    EXPECT_EQ(toCsv(db), first);
    EXPECT_EQ(toCsv(ExportDatabase(db)), first);
}
