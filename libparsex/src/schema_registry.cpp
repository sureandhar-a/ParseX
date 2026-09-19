#include <parsex/schema/schema_registry.hpp>

#include <parsex/schema/atomic_write.hpp>
#include <parsex/schema/cache_layout.hpp>
#include <parsex/schema/xml_schema_raii.hpp>
#include <parsex/telemetry/operation_telemetry.hpp>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace {

struct ParseLog {
    int errorCount = 0;
    std::string firstError;
};

void appendFormatted(ParseLog* log, const char* msg, va_list args) {
    std::array<char, 1024> buf{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): libxml2 hands us a C va_list.
    std::vsnprintf(buf.data(), buf.size(), msg, args);
    if (log->errorCount == 1) {
        log->firstError = buf.data();
    }
}

// NOLINTNEXTLINE(modernize-avoid-variadic-functions): signature mandated by libxml2's xmlSchemaValidityErrorFunc.
void onParseError(void* ctx, const char* msg, ...) {
    auto* log = static_cast<ParseLog*>(ctx);
    ++log->errorCount;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,cppcoreguidelines-init-variables): va_start protocol.
    va_list args;
    va_start(args, msg);
    appendFormatted(log, msg, args);
    va_end(args);
}

// NOLINTNEXTLINE(modernize-avoid-variadic-functions): signature mandated by libxml2's xmlSchemaValidityWarningFunc.
void onParseWarning(void* ctx, const char* msg, ...) {
    (void)ctx;
    (void)msg;
}

std::string readFileBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("resolveSchema: cannot open file: " + path.string());
    }
    std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad()) {
        throw std::runtime_error("resolveSchema: error reading file: " + path.string());
    }
    return bytes;
}

std::string narrowUtf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {utf8.begin(), utf8.end()};
}

XmlSchemaPtr parseSchemaFile(const std::filesystem::path& xsdPath) {
    const std::string narrow = narrowUtf8(xsdPath);
    ParseLog log;
    XmlSchemaParserCtxtPtr parserCtxt{xmlSchemaNewParserCtxt(narrow.c_str())};
    if (parserCtxt == nullptr) {
        throw std::runtime_error(
            "resolveSchema: cannot create schema parser context for: " + xsdPath.string());
    }
    xmlSchemaSetParserErrors(parserCtxt.get(), onParseError, onParseWarning, &log);
    XmlSchemaPtr schema{xmlSchemaParse(parserCtxt.get())};
    if (schema == nullptr) {
        throw std::runtime_error(
            "resolveSchema: cannot parse schema: " + xsdPath.string() +
            (log.firstError.empty() ? "" : " (" + log.firstError + ")"));
    }
    return schema;
}

// First <release>.xsd found wins: $PARSEX_SCHEMA_DIR, then the baked install
// default, then the source tree (dev convenience). Throws when no source has
// the release. (Interim std::runtime_error; SchemaResolutionError next.)
std::filesystem::path findSourceFile(const std::string& release) {
    const std::string filename = release + ".xsd";
    if (const char* env = std::getenv("PARSEX_SCHEMA_DIR"); env != nullptr && *env != '\0') {
        const auto candidate = std::filesystem::path(env) / filename;
        std::error_code dirError;
        if (std::filesystem::is_regular_file(candidate, dirError)) {
            return candidate;
        }
    }
    for (const char* baked : {PARSEX_DEFAULT_SCHEMA_DIR, PARSEX_SOURCE_SCHEMA_DIR}) {
        const auto candidate = std::filesystem::path(baked) / filename;
        std::error_code dirError;
        if (std::filesystem::is_regular_file(candidate, dirError)) {
            return candidate;
        }
    }
    throw std::runtime_error(
        "resolveSchema: unsupported AUTOSAR release: '" + release +
        "' (no " + filename + " in schema sources; see resources/schemas/README.md)");
}

}  // namespace

SchemaResolutionResult resolveSchema(const std::string& release, OperationTelemetry* telemetry) {
    const std::filesystem::path cachePath = cachePathFor(release);

    std::error_code cacheError;
    const bool hit = std::filesystem::is_regular_file(cachePath, cacheError);

    // sourceXsdPath records where the parsed bytes were actually read from:
    // the cache on a hit, the user-supplied source on a miss (the bytes are
    // identical — the miss path copies them into the cache first).
    std::filesystem::path originPath;
    if (!hit) {
        const std::filesystem::path sourcePath = findSourceFile(release);
        writeCacheAtomically(cachePath, readFileBytes(sourcePath));
        originPath = sourcePath;
    } else {
        originPath = cachePath;
    }

    XmlSchemaPtr parsed;
    {
        ScopedTimer timer(telemetry, "SchemaRegistry.resolveSchema");
        parsed = parseSchemaFile(cachePath);
    }

    CachedSchema schema;
    schema.autosarRelease = release;
    schema.sourceXsdPath = std::move(originPath);
    schema.schemaHandle = std::shared_ptr<xmlSchema>(parsed.release(), XmlSchemaDeleter{});

    SchemaResolutionResult result;
    result.schema = std::move(schema);
    result.wasCacheHit = hit;
    return result;
}
