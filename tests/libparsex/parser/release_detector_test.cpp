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

TEST(ReleaseDetectorTest, LineBreakInsideValue) {
    // Some XML tools reformat attributes across lines; libxml2 normalizes
    // the break to spaces, and any residual formatting must still split.
    EXPECT_EQ(detectSchemaFilename(makeRootDocument(
                  {{"xsi:schemaLocation",
                    "http://autosar.org/schema/r4.0\nAUTOSAR_00046.xsd"}})),
              std::optional<std::string>("AUTOSAR_00046.xsd"));
    EXPECT_EQ(detectSchemaFilename(makeRootDocument(
                  {{"xsi:schemaLocation",
                    "http://autosar.org/schema/r4.0\r\n   AUTOSAR_4-2-2.xsd"}})),
              std::optional<std::string>("AUTOSAR_4-2-2.xsd"));
}

TEST(ReleaseDetectorTest, MultilineAttributeFileEndToEnd) {
    // release_multiline.arxml carries a genuine newline + indentation inside
    // xsi:schemaLocation (older-style filename): XML normalization plus the
    // detector must still yield the filename, then the release.
    const auto path =
        std::filesystem::path(PARSEX_FIXTURE_DIR) / "release_multiline.arxml";
    const RawDocument document = loadRawDocument(path);
    const std::optional<std::string> filename = detectSchemaFilename(document);
    EXPECT_EQ(filename, std::optional<std::string>("AUTOSAR_4-2-2.xsd"));
    if (filename.has_value()) {
        EXPECT_EQ(resolveRelease(*filename), "4.2.2");
    }
}

TEST(ReleaseDetectorTest, RealFileYieldsNumericFilename) {
    const auto path =
        std::filesystem::path(PARSEX_FIXTURE_DIR) / "system-4.2.arxml";
    const RawDocument document = loadRawDocument(path);
    EXPECT_EQ(detectSchemaFilename(document), std::optional<std::string>("AUTOSAR_00046.xsd"));
}

TEST(ReleaseDetectorTest, FilenameTableCoversSupportedRange) {
    // Every (filename, release) pair mirrors scripts/download_schemas.py and
    // the Schema Registry's vendored <release>.xsd keys.
    EXPECT_EQ(resolveRelease("AUTOSAR_4-2-2.xsd"), "4.2.2");
    EXPECT_EQ(resolveRelease("AUTOSAR_4-3-0.xsd"), "4.3.0");
    EXPECT_EQ(resolveRelease("AUTOSAR_00044.xsd"), "4.3.1");
    EXPECT_EQ(resolveRelease("AUTOSAR_00046.xsd"), "4.4.0");
    EXPECT_EQ(resolveRelease("AUTOSAR_00048.xsd"), "R19-11");
    EXPECT_EQ(resolveRelease("AUTOSAR_00049.xsd"), "R20-11");
    EXPECT_EQ(resolveRelease("AUTOSAR_00050.xsd"), "R21-11");
}

TEST(ReleaseDetectorTest, UnknownFilenameThrowsWithFilenameInMessage) {
    for (const std::string& filename :
         {"AUTOSAR_4-1-3.xsd", "AUTOSAR_00099.xsd", "something.xsd", ""}) {
        try {
            resolveRelease(filename);
            FAIL() << "expected UnsupportedReleaseError for '" << filename << "'";
        } catch (const UnsupportedReleaseError& err) {
            EXPECT_EQ(err.schemaFilename(), filename);
            EXPECT_NE(std::string(err.what()).find(filename), std::string::npos)
                << "message must contain the filename";
        }
    }
}

TEST(ReleaseDetectorTest, RealFileResolvesToRelease) {
    const auto path =
        std::filesystem::path(PARSEX_FIXTURE_DIR) / "system-4.2.arxml";
    const RawDocument document = loadRawDocument(path);
    const std::optional<std::string> filename = detectSchemaFilename(document);
    // cantools' system-4.2 file declares the 4.4.0-era schema despite its name.
    EXPECT_EQ(filename, std::optional<std::string>("AUTOSAR_00046.xsd"));
    if (filename.has_value()) {
        EXPECT_EQ(resolveRelease(*filename), "4.4.0");
    }
}
