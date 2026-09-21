// Model Builder tests: each buildX() against at least one real element from
// each fixture vocabulary — tiny_valid.arxml (study tags) and system-4.2.arxml
// (real 4.2-era CAN vocabulary). Expected values below were read off the
// fixtures, not assumed.

#include <gtest/gtest.h>

#include <parsex/model/common_fields.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/parser/loader.hpp>
#include <parsex/parser/model_builder.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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

// shortName + category + rawSpanRef come through the shared populateCommonFields
// helper: assert the whole triple for one instance of each domain type.
void expectCommon(const CommonFields& common, const RawNode& node,
                  const std::string& shortName,
                  const std::optional<std::string>& category) {
    EXPECT_EQ(common.shortName, shortName);
    EXPECT_EQ(common.category, category);
    EXPECT_EQ(common.rawSpanRef, node.span);
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

class ModelBuilderWarningsTest : public ::testing::Test {
protected:
    void SetUp() override {
        doc_ = loadRawDocument(fixture("warnings_missing_fields.arxml"));
    }
    [[nodiscard]] const RawDocument& doc() const { return doc_; }

private:
    RawDocument doc_{XmlDocPtr(nullptr)};
};

// Asserts a single warning naming the object and field, located at the node.
void expectWarning(const std::vector<Warning>& warnings, const RawNode& node,
                   const std::string& message) {
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.at(0).message, message);
    EXPECT_EQ(warnings.at(0).location, std::optional<RawSpan>(node.span));
}

const RawNode* findNameless(const RawNode& root, const std::string& tag) {
    if (root.tagName == tag && directText(root, "SHORT-NAME").empty()) {
        return &root;
    }
    for (const auto& child : root.children) {
        const RawNode* found = findNameless(*child, tag);
        if (found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

}  // namespace

TEST_F(ModelBuilderTinyTest, Cluster) {
    const RawNode* node = findElement(doc().root, "CLUSTER");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Cluster cluster = buildCluster(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(cluster.common, *node, "CAN_Cluster", std::optional<std::string>("CAN"));
    EXPECT_EQ(cluster.baudrate, std::optional<std::uint32_t>(500000U));
    EXPECT_EQ(cluster.physicalChannels, std::vector<std::string>{"can0"});
}

TEST_F(ModelBuilderRealTest, Cluster) {
    const RawNode* node = findNamed(doc().root, "CAN-CLUSTER", "Cluster0");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Cluster cluster = buildCluster(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(cluster.common, *node, "Cluster0", std::nullopt);
    EXPECT_EQ(cluster.baudrate, std::optional<std::uint32_t>(500000U));
    // Nested CAN-PHYSICAL-CHANNEL under CAN-CLUSTER-CONDITIONAL — and only
    // the channel's own SHORT-NAME, not the triggerings' nested ones.
    EXPECT_EQ(cluster.physicalChannels, std::vector<std::string>{"Pch0"});
}

TEST_F(ModelBuilderTinyTest, EcuInstance) {
    const RawNode* node = findNamed(doc().root, "ECU-INSTANCE", "ECU_A");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const EcuInstance ecu = buildEcuInstance(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(ecu.common, *node, "ECU_A", std::optional<std::string>("ECU"));
    EXPECT_EQ(ecu.connectedChannels, std::vector<std::string>{"can0"});
    EXPECT_EQ(ecu.controllers, std::vector<std::string>{"CanCtrl_1"});
}

TEST_F(ModelBuilderRealTest, EcuInstance) {
    const RawNode* node = findNamed(doc().root, "ECU-INSTANCE", "DJ");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const EcuInstance ecu = buildEcuInstance(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(ecu.common, *node, "DJ", std::nullopt);
    // No CONNECTED-CHANNELS on a system-template ECU-INSTANCE (only
    // ASSOCIATED-COM-I-PDU-GROUP-REFS, which are PDU groups, not channels).
    EXPECT_TRUE(ecu.connectedChannels.empty());
    EXPECT_EQ(ecu.controllers, std::vector<std::string>{"Observe"});
}

TEST_F(ModelBuilderTinyTest, Frame) {
    const RawNode* node = findNamed(doc().root, "FRAME", "Frame_1");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Frame frame = buildFrame(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(frame.common, *node, "Frame_1", std::nullopt);
    EXPECT_EQ(frame.length, 8U);
    EXPECT_EQ(frame.transmitters, std::vector<std::string>{"ECU_A"});
    ASSERT_EQ(frame.pdus.size(), 1U);
    EXPECT_EQ(frame.pdus.at(0).pduShortNameRef, "Pdu_1");
    EXPECT_EQ(frame.pdus.at(0).startPosition, 0U);
}

TEST_F(ModelBuilderRealTest, Frame) {
    const RawNode* node = findNamed(doc().root, "CAN-FRAME", "MultiplexedMessage");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Frame frame = buildFrame(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(frame.common, *node, "MultiplexedMessage", std::nullopt);
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
    std::vector<Warning> warnings;
    const Pdu pdu = buildPdu(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(pdu.common, *node, "Pdu_1", std::nullopt);
    EXPECT_EQ(pdu.length, 8U);
    ASSERT_EQ(pdu.signalMappings.size(), 1U);
    EXPECT_EQ(pdu.signalMappings.at(0).signalShortNameRef, "Signal_1");
    EXPECT_EQ(pdu.signalMappings.at(0).startPosition, 0U);
    EXPECT_EQ(pdu.signalMappings.at(0).byteOrder, ByteOrder::MostSignificantByteFirst);
}

TEST_F(ModelBuilderRealTest, Pdu) {
    const RawNode* node = findNamed(doc().root, "I-SIGNAL-I-PDU", "multiplexed_message_static");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Pdu pdu = buildPdu(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(pdu.common, *node, "multiplexed_message_static", std::nullopt);
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
    std::vector<Warning> warnings;
    const Signal signal = buildSignal(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(signal.common, *node, "Signal_1", std::nullopt);
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
    std::vector<Warning> warnings;
    const Signal signal = buildSignal(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(signal.common, *node, "MultiplexedStatic", std::nullopt);
    // Packing lives on the owning PDU's I-SIGNAL-TO-I-PDU-MAPPING (captured
    // in PduSignalMapping instead) — the node itself carries none.
    EXPECT_EQ(signal.startBit, 0U);
    EXPECT_EQ(signal.bitLength, 3U);
    EXPECT_TRUE(signal.isSigned);
    EXPECT_EQ(signal.initValue, std::optional<double>(7.0));
    EXPECT_TRUE(signal.receivers.empty());
    EXPECT_EQ(signal.valueTable, std::nullopt);
}

TEST_F(ModelBuilderTinyTest, SignalGroup) {
    const RawNode* node = findNamed(doc().root, "SIGNAL-GROUP", "SignalGroup_1");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const SignalGroup group = buildSignalGroup(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(group.common, *node, "SignalGroup_1", std::nullopt);
    EXPECT_EQ(group.members, std::vector<std::string>{"Signal_1"});
}

TEST_F(ModelBuilderRealTest, SignalGroup) {
    const RawNode* node = findNamed(doc().root, "I-SIGNAL-GROUP", "message1Group");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const SignalGroup group = buildSignalGroup(*node, warnings);
    EXPECT_TRUE(warnings.empty());
    expectCommon(group.common, *node, "message1Group", std::nullopt);
    EXPECT_EQ(group.members,
              std::vector<std::string>({"signal1", "signal5", "signal6"}));
}

TEST_F(ModelBuilderWarningsTest, ClusterWithoutBaudrate) {
    const RawNode* node = findNamed(doc().root, "CLUSTER", "NoBaud");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Cluster cluster = buildCluster(*node, warnings);
    EXPECT_FALSE(cluster.baudrate.has_value());
    expectWarning(warnings, *node, "Cluster 'NoBaud' has no baudrate");
}

TEST_F(ModelBuilderWarningsTest, ElementWithoutShortName) {
    const RawNode* node = findNameless(doc().root, "CLUSTER");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Cluster cluster = buildCluster(*node, warnings);
    EXPECT_EQ(cluster.common.shortName, "");
    expectWarning(warnings, *node, "<CLUSTER> element has no SHORT-NAME");
}

TEST_F(ModelBuilderWarningsTest, EcuInstanceWithoutControllersOrChannels) {
    const RawNode* node = findNamed(doc().root, "ECU-INSTANCE", "Lonely");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const EcuInstance ecu = buildEcuInstance(*node, warnings);
    expectWarning(warnings, *node,
                  "EcuInstance 'Lonely' has no controllers and no connected channels");
}

TEST_F(ModelBuilderWarningsTest, FrameWithoutPduMappings) {
    const RawNode* node = findNamed(doc().root, "FRAME", "Empty");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Frame frame = buildFrame(*node, warnings);
    EXPECT_TRUE(frame.pdus.empty());
    expectWarning(warnings, *node, "Frame 'Empty' has no PDU mappings");
}

TEST_F(ModelBuilderWarningsTest, FrameWithoutLength) {
    const RawNode* node = findNamed(doc().root, "FRAME", "NoLen");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Frame frame = buildFrame(*node, warnings);
    EXPECT_EQ(frame.length, 0U);
    expectWarning(warnings, *node, "Frame 'NoLen' has no length");
}

TEST_F(ModelBuilderWarningsTest, PduWithoutLength) {
    const RawNode* node = findNamed(doc().root, "PDU", "NoLenPdu");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Pdu pdu = buildPdu(*node, warnings);
    EXPECT_EQ(pdu.length, 0U);
    expectWarning(warnings, *node, "Pdu 'NoLenPdu' has no length");
}

TEST_F(ModelBuilderWarningsTest, PduWithoutMappings) {
    const RawNode* node = findNamed(doc().root, "PDU", "NoMap");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Pdu pdu = buildPdu(*node, warnings);
    EXPECT_TRUE(pdu.signalMappings.empty());
    expectWarning(warnings, *node, "Pdu 'NoMap' maps no signals");
}

TEST_F(ModelBuilderWarningsTest, SignalWithoutBitLength) {
    const RawNode* node = findNamed(doc().root, "I-SIGNAL", "NoLengthSignal");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Signal signal = buildSignal(*node, warnings);
    EXPECT_EQ(signal.bitLength, 0U);
    expectWarning(warnings, *node, "Signal 'NoLengthSignal' has no bit length");
}

TEST_F(ModelBuilderWarningsTest, BareSystemSignalStaysSilent) {
    // A SYSTEM-SIGNAL's length lives on its I-SIGNAL: even a bare stub has
    // no required length field of its own, so nothing fires. Pins the
    // don't-cry-wolf rule against real files' comment-only stubs.
    const RawNode* node = findNamed(doc().root, "SYSTEM-SIGNAL", "Stub");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const Signal signal = buildSignal(*node, warnings);
    EXPECT_TRUE(warnings.empty());
}

TEST_F(ModelBuilderWarningsTest, SignalGroupWithoutMembers) {
    const RawNode* node = findNamed(doc().root, "SIGNAL-GROUP", "NoMembers");
    ASSERT_NE(node, nullptr);
    std::vector<Warning> warnings;
    const SignalGroup group = buildSignalGroup(*node, warnings);
    EXPECT_TRUE(group.members.empty());
    expectWarning(warnings, *node, "SignalGroup 'NoMembers' has no members");
}

void buildSubtree(const RawNode& node, std::vector<Warning>& warnings) {
    // Mirrors the dispatch the future parseFile() will own: each recognized
    // family builds, everything else is skipped.
    const std::string& tag = node.tagName;
    if (tag == "CLUSTER" || tag == "CAN-CLUSTER") {
        buildCluster(node, warnings);
    } else if (tag == "ECU-INSTANCE") {
        buildEcuInstance(node, warnings);
    } else if (tag == "FRAME" || tag == "CAN-FRAME") {
        buildFrame(node, warnings);
    } else if (tag == "PDU" || tag == "I-SIGNAL-I-PDU") {
        buildPdu(node, warnings);
    } else if (tag == "SYSTEM-SIGNAL" || tag == "I-SIGNAL") {
        buildSignal(node, warnings);
    } else if (tag == "SIGNAL-GROUP" || tag == "I-SIGNAL-GROUP") {
        buildSignalGroup(node, warnings);
    }
    for (const auto& child : node.children) {
        buildSubtree(*child, warnings);
    }
}

TEST(ModelBuilderWarningsSweepTest, HealthyFixturesWarnOnlyWhereDeserved) {
    std::vector<Warning> tinyWarnings;
    buildSubtree(loadRawDocument(fixture("tiny_valid.arxml")).root, tinyWarnings);
    EXPECT_TRUE(tinyWarnings.empty());

    // The real file's MessageWithoutPDU is deliberately mapping-free upstream
    // (cantools test data) — the single warning proves firing works on real
    // input, and its singularity proves silence everywhere else.
    std::vector<Warning> realWarnings;
    buildSubtree(loadRawDocument(fixture("system-4.2.arxml")).root, realWarnings);
    ASSERT_EQ(realWarnings.size(), 1U);
    EXPECT_EQ(realWarnings.at(0).message, "Frame 'MessageWithoutPDU' has no PDU mappings");
    EXPECT_TRUE(realWarnings.at(0).location.has_value());
}
