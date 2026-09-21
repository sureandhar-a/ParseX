// PAR-99: validateSchema() plumbing — valid fixture is clean, broken fixture
// reports structured errors. Location attribution is PAR-100's job.
#include <gtest/gtest.h>

#include <parsex/schema/xml_schema_raii.hpp>
#include <parsex/validator/validator.hpp>

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

XmlSchemaPtr loadMiniSchema() {
    const std::string path = fixture("mini_test_schema.xsd").string();
    XmlSchemaParserCtxtPtr ctxt(xmlSchemaNewParserCtxt(path.c_str()));
    EXPECT_NE(ctxt, nullptr);
    XmlSchemaPtr schema(xmlSchemaParse(ctxt.get()));
    EXPECT_NE(schema, nullptr);
    return schema;
}

std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    return path;
}

constexpr const char* kValidMini =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<MiniRoot xmlns=\"http://parsex.test/mini\">hello</MiniRoot>\n";
constexpr const char* kBrokenMini =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<MiniRoot xmlns=\"http://parsex.test/mini\"><Child/></MiniRoot>\n";

}  // namespace

TEST(ValidateSchemaTest, ValidFixtureProducesEmptyResult) {
    XmlSchemaPtr schema = loadMiniSchema();
    ASSERT_NE(schema, nullptr);
    const auto path = writeTemp("parsex_validator_valid.xml", kValidMini);

    ParsedFile file;
    file.sourcePath = path;
    const ValidationResult result = Validator{}.validateSchema(file, schema.get());
    EXPECT_FALSE(result.hasErrors()) << "unexpected: " << (result.errors.empty() ? "" : result.errors.front().message);

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(ValidateSchemaTest, BrokenFixtureProducesStructuredError) {
    XmlSchemaPtr schema = loadMiniSchema();
    ASSERT_NE(schema, nullptr);
    const auto path = writeTemp("parsex_validator_broken.xml", kBrokenMini);

    ParsedFile file;
    file.sourcePath = path;
    const ValidationResult result = Validator{}.validateSchema(file, schema.get());
    ASSERT_TRUE(result.hasErrors());
    EXPECT_EQ(result.errors.front().code, "schema.invalid");
    EXPECT_EQ(result.errors.front().severity, Severity::Error);
    EXPECT_FALSE(result.errors.front().message.empty());

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(ValidateSchemaTest, NullSchemaReportsError) {
    ParsedFile file;
    file.sourcePath = fixture("tiny_valid.arxml");
    const ValidationResult result = Validator{}.validateSchema(file, nullptr);
    EXPECT_TRUE(result.hasErrors());
}
