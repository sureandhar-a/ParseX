// Documented-limitation test (PAR-156): the Write Engine faithfully
// round-trips everything the domain model represents — but, like the
// autosar-data crate's schema-typed (non-passthrough) design, it does NOT
// preserve XML comments or elements/attributes outside the domain model.
// This test asserts that absence as EXPECTED, intentional behavior (see the
// Feature-level "Known, up-front limitation" on PAR-138), so a future
// contributor never mistakes it for a bug.

#include <gtest/gtest.h>

#include <parsex/parser/parser.hpp>
#include <parsex/write/write_engine.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    return path;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Hand-crafted fixture: a comment before a SHORT-NAME (the common tooling
// convention) plus an element the domain model does not capture
// (UNMODELED-EXTENSION with an unmodeled attribute).
constexpr const char* kLimitationFixture =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<AUTOSAR xmlns=\"http://autosar.org/schema/r4.0\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xsi:schemaLocation=\"http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd\">\n"
    "  <AR-PACKAGES>\n"
    "    <AR-PACKAGE>\n"
    "      <SHORT-NAME>Sys</SHORT-NAME>\n"
    "      <ELEMENTS>\n"
    "        <!-- PRESERVE-ME-NOT: hand-written modeling note -->\n"
    "        <CAN-FRAME UNMODELED-ATTR=\"unmodeled-value\">\n"
    "          <SHORT-NAME>FrameWithComment</SHORT-NAME>\n"
    "          <FRAME-LENGTH>8</FRAME-LENGTH>\n"
    "          <UNMODELED-EXTENSION>unmodeled-content</UNMODELED-EXTENSION>\n"
    "        </CAN-FRAME>\n"
    "      </ELEMENTS>\n"
    "    </AR-PACKAGE>\n"
    "  </AR-PACKAGES>\n"
    "</AUTOSAR>\n";

}  // namespace

TEST(DocumentLimitationTest, CommentsAndUnmodeledContentAreDroppedByDesign) {
    const auto input = writeTemp("parsex_limitation_in.arxml", kLimitationFixture);
    const ParsedFile parsed = Parser{}.parseFile(input);
    ASSERT_EQ(parsed.frames.size(), 1U);
    EXPECT_EQ(parsed.frames.front().common.shortName, "FrameWithComment");

    ParsedProject project;
    project.files.push_back(parsed);
    const auto output = std::filesystem::temp_directory_path() / "parsex_limitation_out.arxml";
    WriteEngine{}.write(project, output);
    const std::string written = readFile(output);

    // Expected absence (intentional scope, not a bug): the comment text, the
    // unmodeled attribute, and the unmodeled element are all gone...
    EXPECT_EQ(written.find("PRESERVE-ME-NOT"), std::string::npos);
    EXPECT_EQ(written.find("UNMODELED-ATTR"), std::string::npos);
    EXPECT_EQ(written.find("UNMODELED-EXTENSION"), std::string::npos);
    EXPECT_EQ(written.find("unmodeled-content"), std::string::npos);
    // ...while everything the domain model represents survives.
    EXPECT_NE(written.find("FrameWithComment"), std::string::npos);
    EXPECT_NE(written.find("FRAME-LENGTH"), std::string::npos);

    std::error_code ignored;
    std::filesystem::remove(input, ignored);
    std::filesystem::remove(output, ignored);
}
