// Secure-reader setup tests: the Loader's reader opens with XXE-safe options
// and parses legitimate files normally. XXE *rejection* (malicious input) is
// covered later by the error-handling PBI; here we prove the hardening does
// not break legitimate parsing.

#include <gtest/gtest.h>

#include <parsex/parser/secure_reader.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

// Drives reader to EOF, collecting element-start tag names in document order.
// Returns the final xmlTextReaderRead() return value: 0 on clean EOF, -1 on
// parse error.
int readAllElements(xmlTextReaderPtr reader, std::vector<std::string>& out) {
    int ret = 0;
    while ((ret = xmlTextReaderRead(reader)) == 1) {
        if (xmlTextReaderNodeType(reader) == 1 /* XML_READER_TYPE_ELEMENT */) {
            const xmlChar* name = xmlTextReaderConstName(reader);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): xmlChar is libxml2's unsigned-char string type.
            out.emplace_back(reinterpret_cast<const char*>(name));
        }
    }
    return ret;
}

}  // namespace

TEST(SecureReaderTest, HardeningFlagsAreExplicit) {
    const auto options = static_cast<unsigned int>(kSecureReaderOptions);
    // Network fetches are blocked outright ...
    EXPECT_NE(options & static_cast<unsigned int>(XML_PARSE_NONET), 0U);
    // ... and entity substitution / external DTD loading stay off (passing
    // neither flag is what keeps external entities unresolved).
    EXPECT_EQ(options & (static_cast<unsigned int>(XML_PARSE_NOENT) |
                         static_cast<unsigned int>(XML_PARSE_DTDLOAD)),
              0U);
}

TEST(SecureReaderTest, SmallFileOpensAndParsesNormally) {
    XmlReaderPtr reader = openSecureReader(fixture("tiny_valid.arxml"));
    ASSERT_NE(reader, nullptr);

    std::vector<std::string> elements;
    EXPECT_EQ(readAllElements(reader.get(), elements), 0);

    ASSERT_FALSE(elements.empty());
    EXPECT_EQ(elements.front(), "AUTOSAR");
    EXPECT_NE(std::ranges::find(elements, "AR-PACKAGES"), elements.end());
    EXPECT_NE(std::ranges::find(elements, "SHORT-NAME"), elements.end());
}

TEST(SecureReaderTest, MissingFileReturnsNull) {
    EXPECT_EQ(openSecureReader(fixture("does-not-exist.arxml")), nullptr);
}
