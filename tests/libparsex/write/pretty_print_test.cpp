// Pretty-print consolidation tests (PAR-153): matches PAR-141's three
// PBI-level "Unit tests" exactly, including a deeply-nested fixture.

#include <gtest/gtest.h>

#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>
#include <parsex/write/write_context.hpp>
#include <parsex/write/write_elements.hpp>
#include <parsex/write/write_format.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

const std::string kNs = "http://autosar.org/schema/r4.0";
const std::string kLocation = "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd";

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

// Deeply-nested structure: Cluster containing EcuInstance containing Frame
// containing Pdu containing Signal (all under one ELEMENTS for depth).
void buildDeepFixture(WriteContext& ctx) {
    xmlNodePtr package = xmlNewChild(ctx.arPackages(), nullptr, BAD_CAST "AR-PACKAGE", nullptr);
    appendTextChild(package, "SHORT-NAME", "Sys");
    xmlNodePtr elements = xmlNewChild(package, nullptr, BAD_CAST "ELEMENTS", nullptr);

    Cluster cluster;
    cluster.common.shortName = "CAN_Cluster";
    cluster.baudrate = 500000U;
    cluster.physicalChannels = {"can0"};
    EcuInstance ecu;
    ecu.common.shortName = "ECU_A";
    ecu.connectedChannels = {"can0"};
    Frame frame;
    frame.common.shortName = "Frame_1";
    frame.length = 8U;
    frame.transmitters = {"ECU_A"};
    frame.pdus = {{.pduShortNameRef = "Pdu_1", .startPosition = 0}};
    Pdu pdu;
    pdu.common.shortName = "Pdu_1";
    pdu.length = 8U;
    pdu.signalMappings = {{.signalShortNameRef = "Signal_1", .startPosition = 0}};
    Signal signal;
    signal.common.shortName = "Signal_1";
    signal.bitLength = 16U;
    signal.receivers = {"ECU_A"};
    SignalGroup group;
    group.common.shortName = "SignalGroup_1";
    group.members = {"Signal_1"};

    xmlAddChild(elements, buildClusterElement(ctx.doc(), cluster));
    xmlAddChild(elements, buildEcuInstanceElement(ctx.doc(), ecu));
    xmlAddChild(elements, buildFrameElement(ctx.doc(), frame));
    xmlAddChild(elements, buildPduElement(ctx.doc(), pdu));
    xmlAddChild(elements, buildSignalElement(ctx.doc(), signal));
    xmlAddChild(elements, buildSignalGroupElement(ctx.doc(), group));
}

bool hasTrailingWhitespaceLine(const std::string& text) {
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            return true;
        }
        if (line.find_first_not_of(" \t\r") == std::string::npos && !line.empty()) {
            return true;  // whitespace-only line
        }
    }
    return false;
}

}  // namespace

TEST(PrettyPrintTest, NoCollapsedSectionsOrTrailingWhitespace) {
    WriteContext ctx(kNs, kLocation);
    buildDeepFixture(ctx);
    const auto path = std::filesystem::temp_directory_path() / "parsex_pretty_a.arxml";
    ASSERT_GT(writeXmlToFile(ctx.doc(), path.string()), 0);
    const std::string text = readFile(path);
    // Indentation present at depth (no collapsed sections).
    EXPECT_NE(text.find("\n      <ELEMENTS>"), std::string::npos);
    EXPECT_NE(text.find("\n        <CAN-CLUSTER>"), std::string::npos);
    EXPECT_NE(text.find("\n          <SHORT-NAME>CAN_Cluster</SHORT-NAME>"), std::string::npos);
    EXPECT_FALSE(hasTrailingWhitespaceLine(text));
    std::filesystem::remove(path);
}

TEST(PrettyPrintTest, SameTreeWrittenTwiceIsByteIdentical) {
    WriteContext first(kNs, kLocation);
    buildDeepFixture(first);
    WriteContext second(kNs, kLocation);
    buildDeepFixture(second);
    const auto pathA = std::filesystem::temp_directory_path() / "parsex_pretty_b1.arxml";
    const auto pathB = std::filesystem::temp_directory_path() / "parsex_pretty_b2.arxml";
    ASSERT_GT(writeXmlToFile(first.doc(), pathA.string()), 0);
    ASSERT_GT(writeXmlToFile(second.doc(), pathB.string()), 0);
    EXPECT_EQ(readFile(pathA), readFile(pathB));
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);
}

TEST(PrettyPrintTest, FirstLineIsXmlDeclaration) {
    WriteContext ctx(kNs, kLocation);
    buildDeepFixture(ctx);
    const auto path = std::filesystem::temp_directory_path() / "parsex_pretty_c.arxml";
    ASSERT_GT(writeXmlToFile(ctx.doc(), path.string()), 0);
    const std::string text = readFile(path);
    EXPECT_EQ(text.substr(0, text.find('\n')), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    std::filesystem::remove(path);
}
