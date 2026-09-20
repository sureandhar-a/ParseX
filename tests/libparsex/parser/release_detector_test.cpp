// Release-detector tests: the schema filename comes from the root element's
// xsi:schemaLocation "<namespace> <filename>" pair, in both AUTOSAR naming
// styles, with an explicit opt-out (nullopt, never a crash) when absent.

#include <gtest/gtest.h>

#include <parsex/parser/loader.hpp>
#include <parsex/parser/release_detector.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

RawDocument makeRootDocument(std::vector<std::pair<std::string, std::string>> attrs) {
    RawDocument document{XmlDocPtr(nullptr)};
    document.root.tagName = "AUTOSAR";
    document.root.attributes = std::move(attrs);
    return document;
}

}  // namespace

TEST(ReleaseDetectorTest, NumericStyleFilename) {
    // The cantools system-4.2.arxml root element, verbatim.
    RawDocument document = makeRootDocument({
        {"xmlns", "http://autosar.org/schema/r4.0"},
        {"xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"},
        {"xsi:schemaLocation", "http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd"},
    });
    EXPECT_EQ(detectSchemaFilename(document), std::optional<std::string>("AUTOSAR_00046.xsd"));
}

TEST(ReleaseDetectorTest, VersionInNameStyleFilename) {
    RawDocument document = makeRootDocument({
        {"xsi:schemaLocation", "http://autosar.org/schema/r4.0 AUTOSAR_4-2-2.xsd"},
    });
    EXPECT_EQ(detectSchemaFilename(document), std::optional<std::string>("AUTOSAR_4-2-2.xsd"));
}

TEST(ReleaseDetectorTest, MissingAttributeReturnsNullopt) {
    EXPECT_EQ(detectSchemaFilename(makeRootDocument({})), std::nullopt);
    EXPECT_EQ(detectSchemaFilename(makeRootDocument({{"xmlns", "http://autosar.org/schema/r4.0"}})),
              std::nullopt);
}

TEST(ReleaseDetectorTest, MalformedValuesReturnNullopt) {
    EXPECT_EQ(detectSchemaFilename(makeRootDocument({{"xsi:schemaLocation", ""}})),
              std::nullopt);
    EXPECT_EQ(
        detectSchemaFilename(makeRootDocument({{"xsi:schemaLocation", "http://autosar.org/schema/r4.0"}})),
        std::nullopt);
    EXPECT_EQ(detectSchemaFilename(makeRootDocument({{"xsi:schemaLocation", "   "}})),
              std::nullopt);
}

TEST(ReleaseDetectorTest, ExtraWhitespaceAroundPair) {
    RawDocument document = makeRootDocument({
        {"xsi:schemaLocation", "  http://autosar.org/schema/r4.0\t AUTOSAR_00046.xsd  "},
    });
    EXPECT_EQ(detectSchemaFilename(document), std::optional<std::string>("AUTOSAR_00046.xsd"));
}

TEST(ReleaseDetectorTest, RealFileYieldsNumericFilename) {
    const auto path =
        std::filesystem::path(PARSEX_FIXTURE_DIR) / "system-4.2.arxml";
    const RawDocument document = loadRawDocument(path);
    EXPECT_EQ(detectSchemaFilename(document), std::optional<std::string>("AUTOSAR_00046.xsd"));
}
