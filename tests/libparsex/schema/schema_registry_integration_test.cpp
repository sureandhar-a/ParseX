// End-to-end proof that the Schema Registry output is genuinely usable for
// real XSD conformance checking: resolve a real release, then tell a
// conformant ARXML file apart from a non-conformant one.
//
// Working example for later code (e.g. the Validator's validateSchema()):
// resolve once, then validate each document with a FRESH validation context
// created from the shared handle — contexts are never shared, per the
// thread-safety contract on CachedSchema.

#include <gtest/gtest.h>
#include <libxml/parser.h>
#include <parsex/raw/raw_document.hpp>
#include <parsex/schema/schema_registry.hpp>
#include <parsex/schema/schema_resolution_error.hpp>
#include <parsex/schema/xml_schema_raii.hpp>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace {

void setEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void unsetEnv(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

class EnvGuard {
public:
    EnvGuard() {
        xdg_ = readEnv("XDG_CACHE_HOME");
        dir_ = readEnv("PARSEX_SCHEMA_DIR");
    }
    ~EnvGuard() {
        restore("XDG_CACHE_HOME", xdg_);
        restore("PARSEX_SCHEMA_DIR", dir_);
    }
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;
    EnvGuard(EnvGuard&&) noexcept = default;
    EnvGuard& operator=(EnvGuard&&) noexcept = default;

private:
    static std::optional<std::string> readEnv(const char* name) {
        if (const char* value = std::getenv(name); value != nullptr) {
            return std::string(value);
        }
        return std::nullopt;
    }
    static void restore(const char* name, const std::optional<std::string>& saved) {
        if (saved.has_value()) {
            setEnv(name, *saved);
        } else {
            unsetEnv(name);
        }
    }

    std::optional<std::string> xdg_;
    std::optional<std::string> dir_;
};

struct ValidationLog {
    int errorCount = 0;
    std::string lastMessage;
};

void appendFormatted(ValidationLog* log, const char* msg, va_list args) {
    std::array<char, 1024> buf{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): libxml2 hands us a C va_list.
    std::vsnprintf(buf.data(), buf.size(), msg, args);
    log->lastMessage = buf.data();
}

// NOLINTNEXTLINE(modernize-avoid-variadic-functions): signature mandated by libxml2's xmlSchemaValidityErrorFunc.
void onValidError(void* ctx, const char* msg, ...) {
    auto* log = static_cast<ValidationLog*>(ctx);
    ++log->errorCount;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,cppcoreguidelines-init-variables): va_start protocol.
    va_list args;
    va_start(args, msg);
    appendFormatted(log, msg, args);
    va_end(args);
}

// NOLINTNEXTLINE(modernize-avoid-variadic-functions): signature mandated by libxml2's xmlSchemaValidityWarningFunc.
void onValidWarning(void* ctx, const char* msg, ...) {
    (void)ctx;
    (void)msg;
}

std::string narrowUtf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {utf8.begin(), utf8.end()};
}

// Loads a fixture as an xmlDocPtr via the shared-types RAII wrapper.
XmlDocPtr loadFixture(const std::string& filename) {
    const auto path = std::filesystem::path(PARSEX_FIXTURE_DIR) / filename;
    XmlDocPtr doc{xmlReadFile(narrowUtf8(path).c_str(), nullptr, 0)};
    return doc;
}

// Validates one document with its own fresh context (never a shared one).
int validateWithFreshContext(
    const std::shared_ptr<xmlSchema>& schema, xmlDoc* doc, ValidationLog* log) {
    XmlSchemaValidCtxtPtr validCtxt{xmlSchemaNewValidCtxt(schema.get())};
    if (validCtxt == nullptr) {
        return -1;
    }
    xmlSchemaSetValidErrors(validCtxt.get(), onValidError, onValidWarning, log);
    return xmlSchemaValidateDoc(validCtxt.get(), doc);
}

// Resolves 4.2.2, yielding nullopt on clones without user-supplied schemas
// (the test then skips). Any other failure propagates.
std::optional<SchemaResolutionResult> tryResolve() {
    try {
        return resolveSchema("4.2.2");
    } catch (const SchemaResolutionError& err) {
        if (err.reason() != SchemaResolutionReason::UnsupportedRelease) {
            throw;
        }
        return std::nullopt;
    }
}

void expectValidatesCleanly(const std::shared_ptr<xmlSchema>& schema, const std::string& fixture) {
    XmlDocPtr doc = loadFixture(fixture);
    ASSERT_NE(doc, nullptr);
    ValidationLog log;
    EXPECT_EQ(validateWithFreshContext(schema, doc.get(), &log), 0)
        << "last validation error: " << log.lastMessage;
    EXPECT_EQ(log.errorCount, 0);
}

void expectFailsValidation(const std::shared_ptr<xmlSchema>& schema, const std::string& fixture) {
    XmlDocPtr doc = loadFixture(fixture);
    ASSERT_NE(doc, nullptr);
    ValidationLog log;
    EXPECT_NE(validateWithFreshContext(schema, doc.get(), &log), 0);
    EXPECT_GT(log.errorCount, 0);
}

}  // namespace

TEST(SchemaRegistryIntegrationTest, TellsConformantApartFromNonConformant) {
    EnvGuard guard;
    const auto scratchRoot =
        std::filesystem::temp_directory_path() / "parsex_schema_integration_test";
    // Cleared at START (not just end): a previous aborted run must never
    // poison this one with a stale cache entry.
    std::error_code errCode;
    std::filesystem::remove_all(scratchRoot, errCode);
    // Hermetic cache, real user-supplied sources (see resources/schemas/README.md).
    setEnv("XDG_CACHE_HOME", scratchRoot.string());
    setEnv("PARSEX_SCHEMA_DIR", PARSEX_SCHEMA_DIR);

    const std::optional<SchemaResolutionResult> miss = tryResolve();
    if (!miss.has_value()) {
        GTEST_SKIP() << "user-supplied 4.2.2 schema not present";
    }
    EXPECT_FALSE(miss->wasCacheHit);
    ASSERT_NE(miss->schema.schemaHandle, nullptr);

    // Second call: pure cache hit — no source dependency, fresh parse of the
    // cache entry. Each call owns an independent schema object.
    const SchemaResolutionResult hit = resolveSchema("4.2.2");
    EXPECT_TRUE(hit.wasCacheHit);
    ASSERT_NE(hit.schema.schemaHandle, nullptr);
    EXPECT_NE(hit.schema.schemaHandle.get(), miss->schema.schemaHandle.get());

    // Destruction order exercised below (verified by ASan/UBSan, not by
    // asserts): each helper's valid-ctxt dies before its doc, which dies
    // before the shared schemas; the parser contexts died inside
    // resolveSchema() long before any of this.
    expectValidatesCleanly(hit.schema.schemaHandle, "schema_valid.arxml");
    expectFailsValidation(hit.schema.schemaHandle, "schema_invalid.arxml");

    std::filesystem::remove_all(scratchRoot, errCode);
}
