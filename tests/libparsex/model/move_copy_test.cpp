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
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

ParsedFile makePopulatedParsedFile(const std::filesystem::path& fixturePath) {
    ParsedFile file;
    file.autosarRelease = "4.4.0";
    file.sourcePath = fixturePath;
    file.rawDocument = std::make_shared<RawDocument>(loadRawDocument(fixturePath));

    Cluster cluster;
    cluster.common.shortName = "CAN_Cluster";
    cluster.common.category = "CAN";
    cluster.common.rawSpanRef = RawSpan{.startOffset = 10U, .endOffset = 100U, .lineNumber = 1U};
    cluster.baudrate = 500000U;
    cluster.physicalChannels = {"can0"};
    file.clusters.push_back(std::move(cluster));

    EcuInstance ecu;
    ecu.common.shortName = "ECU_A";
    ecu.common.category = "ECU";
    ecu.common.rawSpanRef = RawSpan{.startOffset = 110U, .endOffset = 200U, .lineNumber = 5U};
    ecu.connectedChannels = {"can0"};
    ecu.controllers = {"CanCtrl_1"};
    file.ecuInstances.push_back(std::move(ecu));

    Frame frame;
    frame.common.shortName = "Frame_1";
    frame.common.rawSpanRef = RawSpan{.startOffset = 210U, .endOffset = 300U, .lineNumber = 10U};
    frame.length = 8;
    frame.transmitters = {"ECU_A"};
    FramePduMapping pduMapping;
    pduMapping.pduShortNameRef = "Pdu_1";
    pduMapping.startPosition = 0;
    frame.pdus.push_back(std::move(pduMapping));
    file.frames.push_back(std::move(frame));

    Pdu pdu;
    pdu.common.shortName = "Pdu_1";
    pdu.common.rawSpanRef = RawSpan{.startOffset = 310U, .endOffset = 400U, .lineNumber = 15U};
    pdu.length = 8;
    PduSignalMapping signalMapping;
    signalMapping.signalShortNameRef = "Signal_1";
    signalMapping.startPosition = 0;
    signalMapping.byteOrder = ByteOrder::MostSignificantByteFirst;
    pdu.signalMappings.push_back(std::move(signalMapping));
    file.pdus.push_back(std::move(pdu));

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
    file.signals.push_back(std::move(signal));

    SignalGroup signalGroup;
    signalGroup.common.shortName = "SignalGroup_1";
    signalGroup.common.rawSpanRef = RawSpan{.startOffset = 560U, .endOffset = 600U, .lineNumber = 30U};
    signalGroup.members = {"Signal_1"};
    file.signalGroups.push_back(std::move(signalGroup));

    Warning warning;
    warning.message = "test warning";
    file.warnings.push_back(std::move(warning));

    return file;
}

// Plain aggregates have no operator==, so compare value equality field by field.
template <typename T>
void expectSameSize(const std::vector<T>& lhs, const std::vector<T>& rhs) {
    ASSERT_EQ(lhs.size(), rhs.size());
}

void expectClusterEqual(const Cluster& lhs, const Cluster& rhs) {
    EXPECT_EQ(lhs.common.shortName, rhs.common.shortName);
    EXPECT_EQ(lhs.baudrate, rhs.baudrate);
    EXPECT_EQ(lhs.physicalChannels, rhs.physicalChannels);
}

void expectEcuEqual(const EcuInstance& lhs, const EcuInstance& rhs) {
    EXPECT_EQ(lhs.common.shortName, rhs.common.shortName);
    EXPECT_EQ(lhs.connectedChannels, rhs.connectedChannels);
    EXPECT_EQ(lhs.controllers, rhs.controllers);
}

void expectFrameEqual(const Frame& lhs, const Frame& rhs) {
    EXPECT_EQ(lhs.common.shortName, rhs.common.shortName);
    EXPECT_EQ(lhs.length, rhs.length);
    ASSERT_EQ(lhs.pdus.size(), rhs.pdus.size());
    for (std::size_t i = 0; i < lhs.pdus.size(); ++i) {
        EXPECT_EQ(lhs.pdus.at(i).pduShortNameRef, rhs.pdus.at(i).pduShortNameRef);
        EXPECT_EQ(lhs.pdus.at(i).startPosition, rhs.pdus.at(i).startPosition);
    }
}

void expectPduEqual(const Pdu& lhs, const Pdu& rhs) {
    EXPECT_EQ(lhs.common.shortName, rhs.common.shortName);
    ASSERT_EQ(lhs.signalMappings.size(), rhs.signalMappings.size());
    for (std::size_t i = 0; i < lhs.signalMappings.size(); ++i) {
        EXPECT_EQ(lhs.signalMappings.at(i).signalShortNameRef, rhs.signalMappings.at(i).signalShortNameRef);
    }
}

void expectValueTableEqual(
    const std::vector<ValueTableEntry>& lhs, const std::vector<ValueTableEntry>& rhs) {
    expectSameSize(lhs, rhs);
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        EXPECT_EQ(lhs.at(i).value, rhs.at(i).value);
        EXPECT_EQ(lhs.at(i).label, rhs.at(i).label);
    }
}

void expectSignalEqual(const Signal& lhs, const Signal& rhs) {
    EXPECT_EQ(lhs.common.shortName, rhs.common.shortName);
    EXPECT_EQ(lhs.startBit, rhs.startBit);
    EXPECT_EQ(lhs.bitLength, rhs.bitLength);
    if (!lhs.valueTable.has_value() || !rhs.valueTable.has_value()) {
        FAIL() << "expected valueTable to hold a value on both sides";
        return;
    }
    expectValueTableEqual(*lhs.valueTable, *rhs.valueTable);
}

void expectSignalGroupEqual(const SignalGroup& lhs, const SignalGroup& rhs) {
    EXPECT_EQ(lhs.common.shortName, rhs.common.shortName);
    EXPECT_EQ(lhs.members, rhs.members);
}

void expectParsedFileVectorsEqual(const ParsedFile& lhs, const ParsedFile& rhs) {
    expectSameSize(lhs.clusters, rhs.clusters);
    expectSameSize(lhs.ecuInstances, rhs.ecuInstances);
    expectSameSize(lhs.frames, rhs.frames);
    expectSameSize(lhs.pdus, rhs.pdus);
    expectSameSize(lhs.signals, rhs.signals);
    expectSameSize(lhs.signalGroups, rhs.signalGroups);
    expectSameSize(lhs.warnings, rhs.warnings);

    expectClusterEqual(lhs.clusters.at(0), rhs.clusters.at(0));
    expectEcuEqual(lhs.ecuInstances.at(0), rhs.ecuInstances.at(0));
    expectFrameEqual(lhs.frames.at(0), rhs.frames.at(0));
    expectPduEqual(lhs.pdus.at(0), rhs.pdus.at(0));
    expectSignalEqual(lhs.signals.at(0), rhs.signals.at(0));
    expectSignalGroupEqual(lhs.signalGroups.at(0), rhs.signalGroups.at(0));
}

void expectParsedFileContentsEqual(const ParsedFile& lhs, const ParsedFile& rhs) {
    EXPECT_EQ(lhs.autosarRelease, rhs.autosarRelease);
    EXPECT_EQ(lhs.sourcePath, rhs.sourcePath);
    expectParsedFileVectorsEqual(lhs, rhs);
    EXPECT_EQ(lhs.warnings.at(0).message, rhs.warnings.at(0).message);
}

Frame makePopulatedFrame() {
    Frame frame;
    frame.common.shortName = "Frame_1";
    frame.common.rawSpanRef = RawSpan{.startOffset = 210U, .endOffset = 300U, .lineNumber = 10U};
    frame.length = 8;
    frame.transmitters = {"ECU_A"};
    FramePduMapping first;
    first.pduShortNameRef = "Pdu_1";
    first.startPosition = 0;
    FramePduMapping second;
    second.pduShortNameRef = "Pdu_2";
    second.startPosition = 32U;
    frame.pdus.push_back(std::move(first));
    frame.pdus.push_back(std::move(second));
    return frame;
}

}  // namespace

TEST(MoveCopyTest, CopySharesRawDocumentAndDeepCopiesVectors) {
    const std::filesystem::path fixturePath =
        std::filesystem::path(PARSEX_FIXTURE_DIR) / "tiny_valid.arxml";
    ASSERT_TRUE(std::filesystem::exists(fixturePath)) << "Fixture file missing: " << fixturePath;

    ParsedFile original = makePopulatedParsedFile(fixturePath);
    ASSERT_NE(original.rawDocument, nullptr);
    EXPECT_EQ(original.rawDocument.use_count(), 1L);

    ParsedFile copy = original;

    // Intentional sharing: both point at the same RawDocument.
    EXPECT_EQ(copy.rawDocument.get(), original.rawDocument.get());
    EXPECT_EQ(original.rawDocument.use_count(), 2L);
    EXPECT_EQ(copy.rawDocument.use_count(), 2L);

    // Value equality, not identity, for the vectors.
    expectParsedFileContentsEqual(copy, original);

    // Vectors are deep-copied: mutating the copy must not affect the original.
    copy.clusters.at(0).common.shortName = "Mutated_Cluster";
    copy.frames.at(0).pdus.at(0).pduShortNameRef = "Mutated_Pdu";
    EXPECT_EQ(original.clusters.at(0).common.shortName, "CAN_Cluster");
    EXPECT_EQ(original.frames.at(0).pdus.at(0).pduShortNameRef, "Pdu_1");
}

TEST(MoveCopyTest, MoveTransfersContentsAndLeavesSourceValid) {
    const std::filesystem::path fixturePath =
        std::filesystem::path(PARSEX_FIXTURE_DIR) / "tiny_valid.arxml";
    ASSERT_TRUE(std::filesystem::exists(fixturePath)) << "Fixture file missing: " << fixturePath;

    ParsedFile original = makePopulatedParsedFile(fixturePath);
    const RawDocument* expectedDoc = original.rawDocument.get();

    ParsedFile moved = std::move(original);

    // The moved-to object holds the original contents.
    EXPECT_EQ(moved.clusters.size(), 1U);
    EXPECT_EQ(moved.ecuInstances.size(), 1U);
    EXPECT_EQ(moved.frames.size(), 1U);
    EXPECT_EQ(moved.pdus.size(), 1U);
    EXPECT_EQ(moved.signals.size(), 1U);
    EXPECT_EQ(moved.signalGroups.size(), 1U);
    EXPECT_EQ(moved.clusters.at(0).common.shortName, "CAN_Cluster");
    EXPECT_EQ(moved.frames.at(0).pdus.at(0).pduShortNameRef, "Pdu_1");
    EXPECT_EQ(moved.signals.at(0).common.shortName, "Signal_1");
    ASSERT_NE(moved.rawDocument, nullptr);
    EXPECT_EQ(moved.rawDocument.get(), expectedDoc);
    EXPECT_EQ(moved.rawDocument.use_count(), 1L);

    // Moved-from state is valid-but-unspecified: only destroy or reassign it.
    // Reusing `original` below is the assertion itself, not an accident.
    // NOLINTBEGIN(bugprone-use-after-move)
    original.clusters.clear();
    original.frames.clear();
    original.rawDocument.reset();
    EXPECT_EQ(original.clusters.size(), 0U);
    EXPECT_EQ(original.rawDocument, nullptr);
    original = moved;
    // NOLINTEND(bugprone-use-after-move)
    expectParsedFileContentsEqual(original, moved);
}

TEST(MoveCopyTest, FrameCopyAndMove) {
    Frame original = makePopulatedFrame();

    Frame copy = original;
    ASSERT_EQ(copy.pdus.size(), original.pdus.size());
    EXPECT_EQ(copy.common.shortName, original.common.shortName);
    EXPECT_EQ(copy.pdus.at(0).pduShortNameRef, original.pdus.at(0).pduShortNameRef);
    EXPECT_EQ(copy.pdus.at(1).pduShortNameRef, original.pdus.at(1).pduShortNameRef);

    copy.pdus.at(0).pduShortNameRef = "Mutated_Pdu";
    EXPECT_EQ(original.pdus.at(0).pduShortNameRef, "Pdu_1");

    Frame moved = std::move(original);
    EXPECT_EQ(moved.common.shortName, "Frame_1");
    ASSERT_EQ(moved.pdus.size(), 2U);
    EXPECT_EQ(moved.pdus.at(0).pduShortNameRef, "Pdu_1");
    EXPECT_EQ(moved.pdus.at(1).pduShortNameRef, "Pdu_2");

    // Moved-from Frame stays valid: destroy or reassign only.
    // NOLINTBEGIN(bugprone-use-after-move)
    original.pdus.clear();
    original = moved;
    // NOLINTEND(bugprone-use-after-move)
    EXPECT_EQ(original.common.shortName, "Frame_1");
    ASSERT_EQ(original.pdus.size(), 2U);
    EXPECT_EQ(original.pdus.at(1).pduShortNameRef, "Pdu_2");
}
