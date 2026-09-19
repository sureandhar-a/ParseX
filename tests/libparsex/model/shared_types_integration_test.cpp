#include <gtest/gtest.h>
#include <libxml/tree.h>
#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/parsed_file.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>
#include <parsex/raw/raw_document.hpp>
#include <filesystem>
#include <memory>
#include <string>

namespace {

RawDocument loadRawDocument(const std::filesystem::path& path) {
    // path::c_str() is wchar_t on Windows and u8string() is char8_t here,
    // but libxml2 wants UTF-8 char: copy the UTF-8 code units byte-for-byte.
    // (One named temporary: each u8string() call returns its own buffer.)
    const auto utf8Path = path.u8string();
    const std::string narrowPath{utf8Path.begin(), utf8Path.end()};
    xmlDoc* rawDoc = xmlReadFile(narrowPath.c_str(), nullptr, 0);
    EXPECT_NE(rawDoc, nullptr) << "Failed to parse XML fixture: " << path;
    return RawDocument{XmlDocPtr(rawDoc, XmlDocDeleter{})};
}

}  // namespace

TEST(SharedTypesIntegrationTest, ParsedFileConstructsAndDestructsCleanly) {
    const std::filesystem::path fixturePath =
        std::filesystem::path(PARSEX_FIXTURE_DIR) / "tiny_valid.arxml";
    ASSERT_TRUE(std::filesystem::exists(fixturePath)) << "Fixture file missing: " << fixturePath;

    ParsedFile parsedFile;
    parsedFile.autosarRelease = "4.4.0";
    parsedFile.sourcePath = fixturePath;
    parsedFile.rawDocument = std::make_shared<RawDocument>(loadRawDocument(fixturePath));

    Cluster cluster;
    cluster.common.shortName = "CAN_Cluster";
    cluster.common.category = "CAN";
    cluster.common.rawSpanRef = RawSpan{.startOffset = 10U, .endOffset = 100U, .lineNumber = 1U};
    cluster.baudrate = 500000U;
    cluster.physicalChannels = {"can0"};
    parsedFile.clusters.push_back(std::move(cluster));

    EcuInstance ecu;
    ecu.common.shortName = "ECU_A";
    ecu.common.category = "ECU";
    ecu.common.rawSpanRef = RawSpan{.startOffset = 110U, .endOffset = 200U, .lineNumber = 5U};
    ecu.connectedChannels = {"can0"};
    ecu.controllers = {"CanCtrl_1"};
    parsedFile.ecuInstances.push_back(std::move(ecu));

    Frame frame;
    frame.common.shortName = "Frame_1";
    frame.common.rawSpanRef = RawSpan{.startOffset = 210U, .endOffset = 300U, .lineNumber = 10U};
    frame.length = 8;
    frame.transmitters = {"ECU_A"};
    FramePduMapping pduMapping;
    pduMapping.pduShortNameRef = "Pdu_1";
    pduMapping.startPosition = 0;
    frame.pdus.push_back(std::move(pduMapping));
    parsedFile.frames.push_back(std::move(frame));

    Pdu pdu;
    pdu.common.shortName = "Pdu_1";
    pdu.common.rawSpanRef = RawSpan{.startOffset = 310U, .endOffset = 400U, .lineNumber = 15U};
    pdu.length = 8;
    PduSignalMapping signalMapping;
    signalMapping.signalShortNameRef = "Signal_1";
    signalMapping.startPosition = 0;
    signalMapping.byteOrder = ByteOrder::MostSignificantByteFirst;
    pdu.signalMappings.push_back(std::move(signalMapping));
    parsedFile.pdus.push_back(std::move(pdu));

    Signal signal;
    signal.common.shortName = "Signal_1";
    signal.common.rawSpanRef = RawSpan{.startOffset = 410U, .endOffset = 550U, .lineNumber = 20U};
    signal.startBit = 0;
    signal.bitLength = 16;
    signal.byteOrder = ByteOrder::MostSignificantByteFirst;
    signal.isSigned = false;
    signal.factor = 1.0;
    signal.offset = 0.0;
    signal.min = 0.0;
    signal.max = 65535.0;
    signal.unit = "None";
    signal.initValue = 0.0;
    signal.receivers = {"ECU_A"};
    signal.valueTable = std::vector<ValueTableEntry>{
        ValueTableEntry{.value = 0, .label = "Inactive"},
        ValueTableEntry{.value = 1, .label = "Active"},
    };
    parsedFile.signals.push_back(std::move(signal));

    SignalGroup signalGroup;
    signalGroup.common.shortName = "SignalGroup_1";
    signalGroup.common.rawSpanRef = RawSpan{.startOffset = 560U, .endOffset = 600U, .lineNumber = 30U};
    signalGroup.members = {"Signal_1"};
    parsedFile.signalGroups.push_back(std::move(signalGroup));

    EXPECT_EQ(parsedFile.clusters.size(), 1U);
    EXPECT_EQ(parsedFile.ecuInstances.size(), 1U);
    EXPECT_EQ(parsedFile.frames.size(), 1U);
    EXPECT_EQ(parsedFile.pdus.size(), 1U);
    EXPECT_EQ(parsedFile.signals.size(), 1U);
    EXPECT_EQ(parsedFile.signalGroups.size(), 1U);
    EXPECT_NE(parsedFile.rawDocument, nullptr);
    EXPECT_NE(parsedFile.rawDocument->nativeHandle(), nullptr);
}
