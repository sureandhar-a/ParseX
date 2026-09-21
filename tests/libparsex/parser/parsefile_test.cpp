// parseFile() end-to-end: release detection, tree building, and domain
// construction wired together. Written as the primary usage example — later
// Features build on ParsedFile, and this is where its full shape first gets
// exercised: one of each domain type, cross-referenced the way real AUTOSAR
// files are (Frame_1 -> Pdu_1 -> Signal_1, ECU_A transmitting and receiving).

#include <gtest/gtest.h>

#include <parsex/model/parsed_file.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/parser/release_error.hpp>

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

}  // namespace

TEST(ParseFileTest, CompleteFixtureEndToEnd) {
    const auto path = fixture("parsefile_complete.arxml");
    const ParsedFile file = Parser{}.parseFile(path);

    // Release detection wired in: numeric-style schema filename -> release.
    EXPECT_EQ(file.autosarRelease, "4.4.0");
    EXPECT_EQ(file.sourcePath, path);

    // One of each domain type, no best-effort warnings on healthy input.
    ASSERT_EQ(file.clusters.size(), 1U);
    ASSERT_EQ(file.ecuInstances.size(), 1U);
    ASSERT_EQ(file.frames.size(), 1U);
    ASSERT_EQ(file.pdus.size(), 1U);
    ASSERT_EQ(file.signals.size(), 1U);
    ASSERT_EQ(file.signalGroups.size(), 1U);
    EXPECT_TRUE(file.warnings.empty());

    // Spot-checks follow the cross-reference chain end to end.
    const Cluster& cluster = file.clusters.at(0);
    EXPECT_EQ(cluster.common.shortName, "CAN_Cluster");
    EXPECT_EQ(cluster.baudrate, std::optional<std::uint32_t>(500000U));

    const EcuInstance& ecu = file.ecuInstances.at(0);
    EXPECT_EQ(ecu.common.shortName, "ECU_A");

    const Frame& frame = file.frames.at(0);
    EXPECT_EQ(frame.common.shortName, "Frame_1");
    EXPECT_EQ(frame.length, 8U);
    EXPECT_EQ(frame.transmitters, std::vector<std::string>{"ECU_A"});
    ASSERT_EQ(frame.pdus.size(), 1U);
    EXPECT_EQ(frame.pdus.at(0).pduShortNameRef, "Pdu_1");
    // ... which is the PDU below (name-joined, same file).
    const Pdu& pdu = file.pdus.at(0);
    EXPECT_EQ(pdu.common.shortName, frame.pdus.at(0).pduShortNameRef);
    ASSERT_EQ(pdu.signalMappings.size(), 1U);
    EXPECT_EQ(pdu.signalMappings.at(0).signalShortNameRef, "Signal_1");
    // ... which is the signal below.
    const Signal& signal = file.signals.at(0);
    EXPECT_EQ(signal.common.shortName, pdu.signalMappings.at(0).signalShortNameRef);
    EXPECT_EQ(signal.bitLength, 16U);
    EXPECT_EQ(signal.receivers, std::vector<std::string>{"ECU_A"});

    const SignalGroup& group = file.signalGroups.at(0);
    EXPECT_EQ(group.common.shortName, "SignalGroup_1");
    EXPECT_EQ(group.members, std::vector<std::string>{"Signal_1"});

    // The shared raw tree travels with the file for the Write Engine.
    ASSERT_NE(file.rawDocument, nullptr);
    EXPECT_EQ(file.rawDocument->root.tagName, "AUTOSAR");
}

TEST(ParseFileTest, RealFileEndToEnd) {
    const ParsedFile file = Parser{}.parseFile(fixture("system-4.2.arxml"));

    EXPECT_EQ(file.autosarRelease, "4.4.0");
    EXPECT_EQ(file.clusters.size(), 1U);
    EXPECT_EQ(file.ecuInstances.size(), 3U);
    EXPECT_EQ(file.frames.size(), 8U);
    EXPECT_EQ(file.pdus.size(), 7U);
    EXPECT_EQ(file.signals.size(), 19U + 13U);
    EXPECT_EQ(file.signalGroups.size(), 2U);

    // Exactly one best-effort warning on real input: the deliberately
    // mapping-free MessageWithoutPDU from upstream's test data.
    ASSERT_EQ(file.warnings.size(), 1U);
    EXPECT_EQ(file.warnings.at(0).message, "Frame 'MessageWithoutPDU' has no PDU mappings");
    EXPECT_TRUE(file.warnings.at(0).location.has_value());
}

TEST(ParseFileTest, MissingSchemaLocationThrows) {
    // loader_basic.arxml has no xsi:schemaLocation: release unknowable.
    EXPECT_THROW(Parser{}.parseFile(fixture("loader_basic.arxml")), std::runtime_error);
}

TEST(ParseFileTest, UnsupportedReleaseThrows) {
    // tiny_valid.arxml declares the out-of-range autosar_4_0_0.xsd.
    EXPECT_THROW(Parser{}.parseFile(fixture("tiny_valid.arxml")), UnsupportedReleaseError);
}
