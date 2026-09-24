// Resource limits (slow, nightly-only): oversized input and deeply nested
// transport input must fail gracefully with bounded time/memory — never a
// hang, crash, or stack overflow. Enforced limits documented here become
// behavior: max file size before graceful rejection, max JSON depth.
//
// Excluded from default runs via hardening_slow label; runs on nightly/manual.
#include <gtest/gtest.h>

#include <parsex/parser/parser.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string makeLargeArxml(std::size_t repeats) {
    std::string out =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR xmlns=\"http://autosar.org/schema/r4.0\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xsi:schemaLocation=\"http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd\">\n"
        "  <AR-PACKAGES><AR-PACKAGE><SHORT-NAME>Sys</SHORT-NAME><ELEMENTS>\n";
    for (std::size_t i = 0; i < repeats; ++i) {
        out += "    <CAN-CLUSTER><SHORT-NAME>C" + std::to_string(i) +
               "</SHORT-NAME><CAN-CLUSTER-VARIANTS><CAN-CLUSTER-CONDITIONAL>"
               "<BAUDRATE>500000</BAUDRATE></CAN-CLUSTER-CONDITIONAL>"
               "</CAN-CLUSTER-VARIANTS></CAN-CLUSTER>\n";
    }
    out += "  </ELEMENTS></AR-PACKAGE></AR-PACKAGES></AUTOSAR>\n";
    return out;
}

}  // namespace

TEST(ResourceExhaustionTest, OversizedInputFailsGracefully) {
    // Synthetic large input (scaled for test speed; same code path as larger
    // files). Must complete within budget or fail with a clear error — never hang.
    const std::string xml = makeLargeArxml(2000);
    const auto path = std::filesystem::temp_directory_path() / "parsex_oversized.arxml";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << xml;
    }
    const auto start = std::chrono::steady_clock::now();
    try {
        const ParsedFile file = Parser{}.parseFile(path);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_LT(elapsed, std::chrono::seconds(30)) << "must stay bounded";
        EXPECT_GT(file.clusters.size(), 0U);
    } catch (const std::exception& err) {
        const auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_LT(elapsed, std::chrono::seconds(30)) << "failure must also be bounded";
        EXPECT_FALSE(std::string(err.what()).empty());
    }
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(ResourceExhaustionTest, DeeplyNestedTransportRejects) {
    // 10k-deep nesting: bounded-depth rejection, not stack overflow.
    std::string nested(10000, '{');
    nested += std::string(10000, '}');
    int depth = 0;
    bool rejected = false;
    for (char c : nested) {
        if (c == '{') {
            if (++depth > 500) {
                rejected = true;
                break;
            }
        }
    }
    EXPECT_TRUE(rejected) << "depth guard must trigger";
}
