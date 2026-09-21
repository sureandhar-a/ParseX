// PAR-99: validateSchema() plumbing — valid fixture is clean, broken fixture
// reports structured errors. PAR-100 adds location attribution on top.
#include <gtest/gtest.h>

#include <parsex/parser/loader.hpp>
#include <parsex/schema/xml_schema_raii.hpp>
#include <parsex/validator/validator.hpp>

#include <filesystem>
#include <fstream>
#include <future>

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

// PAR-100: error location maps back to a RawSpan in the source file.
TEST(ValidateSchemaTest, BrokenFixtureErrorCarriesByteSpan) {
    XmlSchemaPtr schema = loadMiniSchema();
    ASSERT_NE(schema, nullptr);
    // Multi-line file so the line->span lookup has something to resolve.
    const std::string broken =
        std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n") +
        "<MiniRoot xmlns=\"http://parsex.test/mini\">\n" + "  <Child/>\n" +
        "</MiniRoot>\n";
    const auto path = writeTemp("parsex_validator_span.xml", broken);

    ParsedFile file;
    file.sourcePath = path;
    file.rawDocument =
        std::make_shared<RawDocument>(loadRawDocument(path));
    const ValidationResult result = Validator{}.validateSchema(file, schema.get());
    ASSERT_TRUE(result.hasErrors());
    ASSERT_TRUE(result.errors.front().location.has_value())
        << "expected byte span, got none: " << result.errors.front().message;
    const RawSpan span = result.errors.front().location.value();
    EXPECT_LT(span.startOffset, span.endOffset);
    EXPECT_GT(span.lineNumber, 0U);

    std::ifstream input(path, std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(input),
                            std::istreambuf_iterator<char>()};
    ASSERT_LT(span.endOffset, bytes.size());
    // The span must slice real element text (not whitespace): it should
    // contain a '<' within a few bytes of its start.
    const std::string window =
        bytes.substr(span.startOffset, std::min<std::size_t>(32, bytes.size() - span.startOffset));
    EXPECT_NE(window.find('<'), std::string::npos) << "window: " << window;

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

// PAR-101: shared xmlSchema* is safe for concurrent validateSchema() calls
// because each call mints its own valid-context. Would catch vctxt-sharing.
TEST(ValidateSchemaTest, ConcurrentCallsShareOneSchemaSafely) {
    XmlSchemaPtr schema = loadMiniSchema();
    ASSERT_NE(schema, nullptr);
    const auto validPath = writeTemp("parsex_validator_conc_valid.xml", kValidMini);
    const auto brokenPath = writeTemp("parsex_validator_conc_broken.xml", kBrokenMini);

    ParsedFile validFile;
    validFile.sourcePath = validPath;
    ParsedFile brokenFile;
    brokenFile.sourcePath = brokenPath;
    const Validator validator;
    ::xmlSchema* shared = schema.get();

    auto runValid = std::async(std::launch::async, [&] {
        return validator.validateSchema(validFile, shared);
    });
    auto runBroken = std::async(std::launch::async, [&] {
        return validator.validateSchema(brokenFile, shared);
    });
    const ValidationResult validResult = runValid.get();
    const ValidationResult brokenResult = runBroken.get();

    EXPECT_FALSE(validResult.hasErrors());
    EXPECT_TRUE(brokenResult.hasErrors());

    std::error_code ignored;
    std::filesystem::remove(validPath, ignored);
    std::filesystem::remove(brokenPath, ignored);
}
