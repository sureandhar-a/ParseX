// Model Builder tests: each buildX() against at least one real element from
// each fixture vocabulary — tiny_valid.arxml (study tags) and system-4.2.arxml
// (real 4.2-era CAN vocabulary). Expected values below were read off the
// fixtures, not assumed.

#include <gtest/gtest.h>

#include <parsex/model/signal.hpp>
#include <parsex/parser/loader.hpp>
#include <parsex/parser/model_builder.hpp>

#include <filesystem>
#include <string>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

std::string trimCopy(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::string directText(const RawNode& node, const std::string& tag) {
    for (const auto& child : node.children) {
        if (child->tagName == tag) {
            return trimCopy(child->text);
        }
    }
    return "";
}

const RawNode* findElement(const RawNode& node, const std::string& tag) {
    if (node.tagName == tag) {
        return &node;
    }
    for (const auto& child : node.children) {
        const RawNode* found = findElement(*child, tag);
        if (found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

const RawNode* findNamed(const RawNode& root, const std::string& tag,
                         const std::string& shortName) {
    if (root.tagName == tag && directText(root, "SHORT-NAME") == shortName) {
        return &root;
    }
    for (const auto& child : root.children) {
        const RawNode* found = findNamed(*child, tag, shortName);
        if (found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

class ModelBuilderTinyTest : public ::testing::Test {
protected:
    void SetUp() override { doc_ = loadRawDocument(fixture("tiny_valid.arxml")); }
    [[nodiscard]] const RawDocument& doc() const { return doc_; }

private:
    RawDocument doc_{XmlDocPtr(nullptr)};
};

class ModelBuilderRealTest : public ::testing::Test {
protected:
    void SetUp() override { doc_ = loadRawDocument(fixture("system-4.2.arxml")); }
    [[nodiscard]] const RawDocument& doc() const { return doc_; }

private:
    RawDocument doc_{XmlDocPtr(nullptr)};
};

}  // namespace

TEST_F(ModelBuilderTinyTest, Cluster) {
    const RawNode* node = findElement(doc().root, "CLUSTER");
    ASSERT_NE(node, nullptr);
    const Cluster cluster = buildCluster(*node);
    EXPECT_EQ(cluster.common.shortName, "CAN_Cluster");
    EXPECT_EQ(cluster.common.category, std::optional<std::string>("CAN"));
    EXPECT_EQ(cluster.common.rawSpanRef, node->span);
    EXPECT_EQ(cluster.baudrate, std::optional<std::uint32_t>(500000U));
    EXPECT_EQ(cluster.physicalChannels, std::vector<std::string>{"can0"});
}

TEST_F(ModelBuilderRealTest, Cluster) {
    const RawNode* node = findNamed(doc().root, "CAN-CLUSTER", "Cluster0");
    ASSERT_NE(node, nullptr);
    const Cluster cluster = buildCluster(*node);
    EXPECT_EQ(cluster.common.shortName, "Cluster0");
    EXPECT_EQ(cluster.common.category, std::nullopt);
    EXPECT_EQ(cluster.baudrate, std::optional<std::uint32_t>(500000U));
    // Nested CAN-PHYSICAL-CHANNEL under CAN-CLUSTER-CONDITIONAL — and only
    // the channel's own SHORT-NAME, not the triggerings' nested ones.
    EXPECT_EQ(cluster.physicalChannels, std::vector<std::string>{"Pch0"});
}

TEST_F(ModelBuilderTinyTest, EcuInstance) {
    const RawNode* node = findNamed(doc().root, "ECU-INSTANCE", "ECU_A");
    ASSERT_NE(node, nullptr);
    const EcuInstance ecu = buildEcuInstance(*node);
    EXPECT_EQ(ecu.common.shortName, "ECU_A");
    EXPECT_EQ(ecu.connectedChannels, std::vector<std::string>{"can0"});
    EXPECT_EQ(ecu.controllers, std::vector<std::string>{"CanCtrl_1"});
}

TEST_F(ModelBuilderRealTest, EcuInstance) {
    const RawNode* node = findNamed(doc().root, "ECU-INSTANCE", "DJ");
    ASSERT_NE(node, nullptr);
    const EcuInstance ecu = buildEcuInstance(*node);
    EXPECT_EQ(ecu.common.shortName, "DJ");
    // No CONNECTED-CHANNELS on a system-template ECU-INSTANCE (only
    // ASSOCIATED-COM-I-PDU-GROUP-REFS, which are PDU groups, not channels).
    EXPECT_TRUE(ecu.connectedChannels.empty());
    EXPECT_EQ(ecu.controllers, std::vector<std::string>{"Observe"});
}

TEST_F(ModelBuilderTinyTest, Frame) {
    const RawNode* node = findNamed(doc().root, "FRAME", "Frame_1");
    ASSERT_NE(node, nullptr);
    const Frame frame = buildFrame(*node);
    EXPECT_EQ(frame.common.shortName, "Frame_1");
    EXPECT_EQ(frame.length, 8U);
    EXPECT_EQ(frame.transmitters, std::vector<std::string>{"ECU_A"});
    ASSERT_EQ(frame.pdus.size(), 1U);
    EXPECT_EQ(frame.pdus.at(0).pduShortNameRef, "Pdu_1");
    EXPECT_EQ(frame.pdus.at(0).startPosition, 0U);
}

TEST_F(ModelBuilderRealTest, Frame) {
    const RawNode* node = findNamed(doc().root, "CAN-FRAME", "MultiplexedMessage");
    ASSERT_NE(node, nullptr);
    const Frame frame = buildFrame(*node);
    EXPECT_EQ(frame.common.shortName, "MultiplexedMessage");
    EXPECT_EQ(frame.length, 2U);
    // No TRANSMITTERS on a CAN-FRAME node — senders live behind
    // FRAME-TRIGGERINGs, joined at project level later.
    EXPECT_TRUE(frame.transmitters.empty());
    ASSERT_EQ(frame.pdus.size(), 1U);
    EXPECT_EQ(frame.pdus.at(0).pduShortNameRef, "multiplexed_message");
    EXPECT_EQ(frame.pdus.at(0).startPosition, 0U);
}

TEST_F(ModelBuilderTinyTest, Pdu) {
    const RawNode* node = findNamed(doc().root, "PDU", "Pdu_1");
    ASSERT_NE(node, nullptr);
    const Pdu pdu = buildPdu(*node);
    EXPECT_EQ(pdu.common.shortName, "Pdu_1");
    EXPECT_EQ(pdu.length, 8U);
    ASSERT_EQ(pdu.signalMappings.size(), 1U);
    EXPECT_EQ(pdu.signalMappings.at(0).signalShortNameRef, "Signal_1");
    EXPECT_EQ(pdu.signalMappings.at(0).startPosition, 0U);
    EXPECT_EQ(pdu.signalMappings.at(0).byteOrder, ByteOrder::MostSignificantByteFirst);
}

TEST_F(ModelBuilderRealTest, Pdu) {
    const RawNode* node = findNamed(doc().root, "I-SIGNAL-I-PDU", "multiplexed_message_static");
    ASSERT_NE(node, nullptr);
    const Pdu pdu = buildPdu(*node);
    EXPECT_EQ(pdu.common.shortName, "multiplexed_message_static");
    EXPECT_EQ(pdu.length, 8U);
    ASSERT_EQ(pdu.signalMappings.size(), 2U);
    EXPECT_EQ(pdu.signalMappings.at(0).signalShortNameRef, "MultiplexedStatic");
    EXPECT_EQ(pdu.signalMappings.at(0).startPosition, 0U);
    EXPECT_EQ(pdu.signalMappings.at(1).signalShortNameRef, "MultiplexedStatic2");
    EXPECT_EQ(pdu.signalMappings.at(1).startPosition, 8U);
    // The file's PACKING-BYTE-ORDER is MOST-SIGNIFICANT-BYTE-LAST, which the
    // domain cannot express — pinned here as the documented default fallback.
    EXPECT_EQ(pdu.signalMappings.at(0).byteOrder, ByteOrder::MostSignificantByteFirst);
}

TEST_F(ModelBuilderTinyTest, Signal) {
    const RawNode* node = findNamed(doc().root, "SYSTEM-SIGNAL", "Signal_1");
    ASSERT_NE(node, nullptr);
    const Signal signal = buildSignal(*node);
    EXPECT_EQ(signal.common.shortName, "Signal_1");
    EXPECT_EQ(signal.startBit, 0U);
    EXPECT_EQ(signal.bitLength, 16U);
    EXPECT_EQ(signal.byteOrder, ByteOrder::MostSignificantByteFirst);
    EXPECT_FALSE(signal.isSigned);
    EXPECT_DOUBLE_EQ(signal.factor, 1.0);
    EXPECT_DOUBLE_EQ(signal.offset, 0.0);
    EXPECT_EQ(signal.min, std::nullopt);
    EXPECT_EQ(signal.max, std::nullopt);
    EXPECT_EQ(signal.unit, std::nullopt);
    EXPECT_EQ(signal.initValue, std::nullopt);
    EXPECT_EQ(signal.receivers, std::vector<std::string>{"ECU_A"});
    const std::vector<ValueTableEntry> entries =
        signal.valueTable.value_or(std::vector<ValueTableEntry>{});
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_EQ(entries.at(0).value, 0);
    EXPECT_EQ(entries.at(0).label, "Inactive");
    EXPECT_EQ(entries.at(1).value, 1);
    EXPECT_EQ(entries.at(1).label, "Active");
}

TEST_F(ModelBuilderRealTest, Signal) {
    const RawNode* node = findNamed(doc().root, "I-SIGNAL", "MultiplexedStatic");
    ASSERT_NE(node, nullptr);
    const Signal signal = buildSignal(*node);
    EXPECT_EQ(signal.common.shortName, "MultiplexedStatic");
    // Packing lives on the owning PDU's I-SIGNAL-TO-I-PDU-MAPPING (captured
    // in PduSignalMapping instead) — the node itself carries none.
    EXPECT_EQ(signal.startBit, 0U);
    EXPECT_EQ(signal.bitLength, 3U);
    EXPECT_TRUE(signal.isSigned);
    EXPECT_EQ(signal.initValue, std::optional<double>(7.0));
    EXPECT_TRUE(signal.receivers.empty());
    EXPECT_EQ(signal.valueTable, std::nullopt);
    EXPECT_EQ(signal.common.rawSpanRef, node->span);
}

TEST_F(ModelBuilderTinyTest, SignalGroup) {
    const RawNode* node = findNamed(doc().root, "SIGNAL-GROUP", "SignalGroup_1");
    ASSERT_NE(node, nullptr);
    const SignalGroup group = buildSignalGroup(*node);
    EXPECT_EQ(group.common.shortName, "SignalGroup_1");
    EXPECT_EQ(group.members, std::vector<std::string>{"Signal_1"});
}

TEST_F(ModelBuilderRealTest, SignalGroup) {
    const RawNode* node = findNamed(doc().root, "I-SIGNAL-GROUP", "message1Group");
    ASSERT_NE(node, nullptr);
    const SignalGroup group = buildSignalGroup(*node);
    EXPECT_EQ(group.common.shortName, "message1Group");
    EXPECT_EQ(group.members,
              std::vector<std::string>({"signal1", "signal5", "signal6"}));
}
