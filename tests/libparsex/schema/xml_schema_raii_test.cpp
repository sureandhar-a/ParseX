// End-to-end smoke test for the schema RAII wrappers: parse a real
// user-supplied XSD into an xmlSchema, build a validation context from it,
// then let everything go out of scope (leak-checked under ASan).
//
// Call order follows the canonical libxml2 sequence
// (xmlSchemaNewParserCtxt -> xmlSchemaParse -> xmlSchemaNewValidCtxt).
// Error callbacks are registered before parsing/validating so libxml2
// reports into our log instead of printing to stderr.

#include <gtest/gtest.h>
#include <libxml/xmlschemas.h>
#include <parsex/schema/xml_schema_raii.hpp>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {

struct ErrorLog {
    int errorCount = 0;
    int warningCount = 0;
    std::string lastMessage;
};

void appendFormatted(ErrorLog* log, const char* msg, va_list args) {
    std::array<char, 1024> buf{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): libxml2 hands us a C va_list.
    std::vsnprintf(buf.data(), buf.size(), msg, args);
    log->lastMessage = buf.data();
}

// NOLINTNEXTLINE(modernize-avoid-variadic-functions): signature mandated by libxml2's xmlSchemaValidityErrorFunc.
void onSchemaError(void* ctx, const char* msg, ...) {
    auto* log = static_cast<ErrorLog*>(ctx);
    ++log->errorCount;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,cppcoreguidelines-init-variables): va_start protocol.
    va_list args;
    va_start(args, msg);
    appendFormatted(log, msg, args);
    va_end(args);
}

// NOLINTNEXTLINE(modernize-avoid-variadic-functions): signature mandated by libxml2's xmlSchemaValidityWarningFunc.
void onSchemaWarning(void* ctx, const char* msg, ...) {
    auto* log = static_cast<ErrorLog*>(ctx);
    ++log->warningCount;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,cppcoreguidelines-init-variables): va_start protocol.
    va_list args;
    va_start(args, msg);
    appendFormatted(log, msg, args);
    va_end(args);
}

}  // namespace

TEST(XmlSchemaRaiiTest, ParseRealXsdAndBuildValidContext) {
    const std::filesystem::path xsdPath =
        std::filesystem::path(PARSEX_SCHEMA_DIR) / "4.2.2.xsd";
    if (!std::filesystem::exists(xsdPath)) {
        GTEST_SKIP() << "user-supplied schema not present: " << xsdPath
                     << " (see resources/schemas/README.md)";
    }

    // libxml2 wants a UTF-8 narrow path: copy the code units byte-for-byte.
    const auto utf8Path = xsdPath.u8string();
    const std::string narrowPath{utf8Path.begin(), utf8Path.end()};

    ErrorLog parserLog;
    XmlSchemaParserCtxtPtr parserCtxt{xmlSchemaNewParserCtxt(narrowPath.c_str())};
    ASSERT_NE(parserCtxt, nullptr) << "xmlSchemaNewParserCtxt failed for " << xsdPath;
    xmlSchemaSetParserErrors(parserCtxt.get(), onSchemaError, onSchemaWarning, &parserLog);

    XmlSchemaPtr schema{xmlSchemaParse(parserCtxt.get())};
    EXPECT_EQ(parserLog.errorCount, 0) << "last parser error: " << parserLog.lastMessage;
    ASSERT_NE(schema, nullptr) << "xmlSchemaParse failed for " << xsdPath;

    // Parser context is done once the schema is parsed; freeing it here
    // (rather than at scope end) proves the schema outlives its parser.
    parserCtxt.reset();

    ErrorLog validLog;
    XmlSchemaValidCtxtPtr validCtxt{xmlSchemaNewValidCtxt(schema.get())};
    ASSERT_NE(validCtxt, nullptr) << "xmlSchemaNewValidCtxt failed";
    xmlSchemaSetValidErrors(validCtxt.get(), onSchemaError, onSchemaWarning, &validLog);

    // Scope exit runs all three deleters: valid ctxt, schema. ASan verifies
    // nothing leaks.
}
