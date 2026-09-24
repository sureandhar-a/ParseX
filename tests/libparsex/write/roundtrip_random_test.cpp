// Seeded random document generator driving the no-op stability check.
// Property-style: many valid-but-varied inputs, reproducible by seed.
// CI runs a modest count (100) for speed; set PARSEX_ROUNDTRIP_SEEDS=N for a
// larger local exploratory sweep (mirrors the fuzz smoke-vs-exploratory split).
#include <gtest/gtest.h>

#include <parsex/diff/diff_engine.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/write/write_engine.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

std::string docForSeed(std::uint32_t seed) {
    std::mt19937 rng(seed);
    auto pick = [&](int lo, int hi) {
        return std::uniform_int_distribution<int>(lo, hi)(rng);
    };
    const int clusters = pick(1, 2);
    const int ecus = pick(1, 2);
    const int frames = pick(1, 3);
    const int pdus = pick(1, 3);
    const int signals = pick(1, 3);
    std::string out =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR xmlns=\"http://autosar.org/schema/r4.0\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xsi:schemaLocation=\"http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd\">\n"
        "  <AR-PACKAGES><AR-PACKAGE><SHORT-NAME>Sys</SHORT-NAME><ELEMENTS>\n";
    for (int i = 0; i < clusters; ++i) {
        out += "    <CAN-CLUSTER><SHORT-NAME>C" + std::to_string(seed) + "_" +
               std::to_string(i) +
               "</SHORT-NAME><CAN-CLUSTER-VARIANTS><CAN-CLUSTER-CONDITIONAL>"
               "<BAUDRATE>" +
               std::to_string(pick(125000, 500000)) +
               "</BAUDRATE></CAN-CLUSTER-CONDITIONAL></CAN-CLUSTER-VARIANTS>"
               "</CAN-CLUSTER>\n";
    }
    for (int i = 0; i < ecus; ++i) {
        out += "    <ECU-INSTANCE><SHORT-NAME>E" + std::to_string(seed) + "_" +
               std::to_string(i) + "</SHORT-NAME></ECU-INSTANCE>\n";
    }
    for (int i = 0; i < frames; ++i) {
        out += "    <CAN-FRAME><SHORT-NAME>F" + std::to_string(seed) + "_" +
               std::to_string(i) + "</SHORT-NAME><FRAME-LENGTH>" +
               std::to_string(pick(1, 8)) + "</FRAME-LENGTH></CAN-FRAME>\n";
    }
    for (int i = 0; i < pdus; ++i) {
        out += "    <I-SIGNAL-I-PDU><SHORT-NAME>P" + std::to_string(seed) + "_" +
               std::to_string(i) + "</SHORT-NAME><LENGTH>" + std::to_string(pick(1, 8)) +
               "</LENGTH></I-SIGNAL-I-PDU>\n";
    }
    for (int i = 0; i < signals; ++i) {
        out += "    <I-SIGNAL><SHORT-NAME>S" + std::to_string(seed) + "_" +
               std::to_string(i) + "</SHORT-NAME><LENGTH>" + std::to_string(pick(1, 8)) +
               "</LENGTH></I-SIGNAL>\n";
    }
    out += "  </ELEMENTS></AR-PACKAGE></AR-PACKAGES></AUTOSAR>\n";
    return out;
}

void checkSeed(std::uint32_t seed) {
    const std::string xml = docForSeed(seed);
    const std::filesystem::path src =
        std::filesystem::temp_directory_path() / ("parsex_rand_" + std::to_string(seed) + ".arxml");
    const std::filesystem::path dst =
        std::filesystem::temp_directory_path() / ("parsex_rand_" + std::to_string(seed) + "_out.arxml");
    {
        std::ofstream out(src, std::ios::binary | std::ios::trunc);
        out << xml;
    }
    ParsedProject original;
    try {
        original.files.push_back(Parser{}.parseFile(src));
    } catch (const std::exception& error) {
        FAIL() << "seed " << seed << " failed to parse (generator bug): " << error.what();
    }
    try {
        WriteEngine{}.write(original, dst);
    } catch (const std::exception& error) {
        FAIL() << "seed " << seed << " failed to write: " << error.what();
    }
    ParsedProject reparsed;
    try {
        reparsed.files.push_back(Parser{}.parseFile(dst));
    } catch (const std::exception& error) {
        FAIL() << "seed " << seed << " reparse failed: " << error.what();
    }
    const DiffReport report = DiffEngine{}.diff(original, reparsed);
    if (!report.empty()) {
        FAIL() << "seed " << seed << " round-trip mismatch:\n" << report.toText();
    }
    std::error_code ignored;
    std::filesystem::remove(src, ignored);
    std::filesystem::remove(dst, ignored);
}

int seedCount() {
    if (const char* env = std::getenv("PARSEX_ROUNDTRIP_SEEDS")) {
        try {
            return std::max(1, std::stoi(env));
        } catch (...) {
        }
    }
    return 100;
}

}  // namespace

TEST(RoundTripRandomTest, SeededDocumentsAreStable) {
    const int count = seedCount();
    for (int i = 0; i < count; ++i) {
        checkSeed(static_cast<std::uint32_t>(1000 + i));
    }
    RecordProperty("seed_count", std::to_string(count));
}
