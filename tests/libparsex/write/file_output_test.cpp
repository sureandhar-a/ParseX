// File-writing tests (PAR-152): xmlSaveFormatFileEnc with XML declaration,
// clear errors on bad paths, consistent indentation.

#include <gtest/gtest.h>

#include <parsex/model/cluster.hpp>
#include <parsex/write/write_context.hpp>
#include <parsex/write/write_elements.hpp>
#include <parsex/write/write_format.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

std::filesystem::path tempFile(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("parsex_write_" + name);
}

}  // namespace

TEST(FileOutputTest, FirstLineIsXmlDeclaration) {
    WriteContext ctx("http://autosar.org/schema/r4.0",
                     "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd");
    Cluster cluster;
    cluster.common.shortName = "C";
    xmlAddChild(ctx.arPackages(), buildClusterElement(ctx.doc(), cluster));
    const auto path = tempFile("decl.arxml");
    ASSERT_GT(writeXmlToFile(ctx.doc(), path.string()), 0);
    const std::string text = readFile(path);
    const std::string firstLine = text.substr(0, text.find('\n'));
    EXPECT_EQ(firstLine, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    std::filesystem::remove(path);
}

TEST(FileOutputTest, UnwritablePathThrowsClearError) {
    WriteContext ctx("http://autosar.org/schema/r4.0",
                     "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd");
    EXPECT_THROW(writeXmlToFile(ctx.doc(), "/nonexistent_dir_xyz/parsex_out.arxml"),
                 std::runtime_error);
}

TEST(FileOutputTest, IndentationIsConsistentAtDepth) {
    WriteContext ctx("http://autosar.org/schema/r4.0",
                     "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd");
    xmlNodePtr package = xmlNewChild(ctx.arPackages(), nullptr, BAD_CAST "AR-PACKAGE", nullptr);
    appendTextChild(package, "SHORT-NAME", "Sys");
    xmlNodePtr elements = xmlNewChild(package, nullptr, BAD_CAST "ELEMENTS", nullptr);
    Cluster cluster;
    cluster.common.shortName = "C";
    cluster.physicalChannels = {"can0"};
    xmlAddChild(elements, buildClusterElement(ctx.doc(), cluster));
    const auto path = tempFile("indent.arxml");
    ASSERT_GT(writeXmlToFile(ctx.doc(), path.string()), 0);
    const std::string text = readFile(path);
    // libxml2 default indent is 2 spaces; every nesting level adds one unit.
    EXPECT_NE(text.find("\n  <AR-PACKAGES>"), std::string::npos);
    EXPECT_NE(text.find("\n    <AR-PACKAGE>"), std::string::npos);
    EXPECT_NE(text.find("\n      <SHORT-NAME>Sys</SHORT-NAME>"), std::string::npos);
    std::filesystem::remove(path);
}
