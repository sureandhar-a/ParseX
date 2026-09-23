// Tests for the kind-specific payload schemas (PAR-173).
//
// Verifies validation_result.schema.json and diff_report.schema.json are
// independently well-formed, and that the envelope schema's payload
// conditional selects the right schema in both directions (validationResult
// vs diffReport) using the real json-schema-validator library (PAR-172)
// with a loader resolving the envelope's relative $refs from schemas/.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

namespace {

namespace fs = std::filesystem;

fs::path schemasDir() {
#ifdef PARSEX_SCHEMAS_DIR
    return fs::path(PARSEX_SCHEMAS_DIR);
#else
    return fs::path("schemas");
#endif
}

nlohmann::json loadJsonFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in.good()) {
        throw std::runtime_error("cannot open " + path.string());
    }
    std::ostringstream raw;
    raw << in.rdbuf();
    return nlohmann::json::parse(raw.str());
}

// Loader mapping the envelope's relative $refs (and $id URLs) to files.
void schemaLoader(const nlohmann::json_uri& uri, nlohmann::json& value) {
    const std::string id = uri.to_string();
    const fs::path dir = schemasDir();
    if (id.find("validation_result") != std::string::npos) {
        value = loadJsonFile(dir / "validation_result.schema.json");
        return;
    }
    if (id.find("diff_report") != std::string::npos) {
        value = loadJsonFile(dir / "diff_report.schema.json");
        return;
    }
    throw std::runtime_error("schemaLoader: unrecognized ref " + id);
}

// The envelope schema uses "format": "uri-reference" (PAR-166, standard in
// draft 2020-12), but this library's default_string_format_check does not
// implement uri-reference and reports it as a failure. Pass through that one
// format and delegate the rest to the default checker.
void parsexFormatChecker(const std::string& format, const std::string& value) {
    if (format == "uri-reference") {
        return;
    }
    nlohmann::json_schema::default_string_format_check(format, value);
}

bool validatesAgainst(const nlohmann::json& schema, const nlohmann::json& doc) {
    nlohmann::json_schema::json_validator validator(schemaLoader, parsexFormatChecker);
    validator.set_root_schema(schema);
    nlohmann::json_schema::basic_error_handler handler;
    validator.validate(doc, handler);
    return !handler;
}

nlohmann::json validationPayload() {
    return nlohmann::json{
        {"passed", false},
        {"errors",
         nlohmann::json::array(
             {nlohmann::json{
                 {"severity", "error"},
                 {"code", "can.dlc_mismatch"},
                 {"message", "DLC 8 != 64"},
                 {"path", "/Cluster/CAN/Frame"},
             }})},
    };
}

nlohmann::json diffPayload() {
    return nlohmann::json{
        {"entries",
         nlohmann::json::array(
             {nlohmann::json{
                 {"kind", "modified"},
                 {"elementType", "Frame"},
                 {"newPath", "/F/Old"},
                 {"fieldDiffs",
                  nlohmann::json::array(
                      {nlohmann::json{
                          {"field", "length"},
                          {"oldValue", "8"},
                          {"newValue", "64"},
                      }})},
             }})},
        {"diagnostics", nlohmann::json::array()},
    };
}

nlohmann::json envelopeFor(const std::string& kind, nlohmann::json payload) {
    return nlohmann::json{
        {"$schema", "https://parsex.dev/schemas/v1/envelope.json"},
        {"contractVersion", "1.0.0"},
        {"toolVersion", "1.0.0"},
        {"kind", kind},
        {"payload", std::move(payload)},
    };
}

}  // namespace

TEST(PayloadSchemasTest, BothSchemasAreWellFormed) {
    const fs::path dir = schemasDir();
    for (const char* file : {"validation_result.schema.json", "diff_report.schema.json"}) {
        const fs::path path = dir / file;
        ASSERT_TRUE(fs::exists(path)) << path;
        nlohmann::json schema = nullptr;
        ASSERT_NO_THROW(schema = loadJsonFile(path)) << path;
        ASSERT_TRUE(schema.is_object()) << file;
        EXPECT_EQ(schema.at("type").get<std::string>(), "object") << file;
        ASSERT_TRUE(schema.contains("properties")) << file;
        ASSERT_TRUE(schema.contains("required")) << file;
        EXPECT_EQ(schema.at("$schema").get<std::string>(),
                  "https://json-schema.org/draft/2020-12/schema")
            << file;
    }

    const nlohmann::json validationSchema = loadJsonFile(dir / "validation_result.schema.json");
    EXPECT_EQ(validationSchema.at("$id").get<std::string>(),
              "https://parsex.dev/schemas/v1/validation_result.json");
    const nlohmann::json diffSchema = loadJsonFile(dir / "diff_report.schema.json");
    EXPECT_EQ(diffSchema.at("$id").get<std::string>(), "https://parsex.dev/schemas/v1/diff_report.json");
}

TEST(PayloadSchemasTest, EnvelopeSelectsValidationResultSchema) {
    const nlohmann::json envelopeSchema = loadJsonFile(schemasDir() / "envelope.schema.json");
    const nlohmann::json doc = envelopeFor("validationResult", validationPayload());
    EXPECT_TRUE(validatesAgainst(envelopeSchema, doc))
        << "kind=validationResult envelope must validate: " << doc.dump(2);
}

TEST(PayloadSchemasTest, EnvelopeSelectsDiffReportSchema) {
    const nlohmann::json envelopeSchema = loadJsonFile(schemasDir() / "envelope.schema.json");
    const nlohmann::json doc = envelopeFor("diffReport", diffPayload());
    EXPECT_TRUE(validatesAgainst(envelopeSchema, doc)) << "kind=diffReport envelope must validate";
}

TEST(PayloadSchemasTest, CrossKindPayloadFailsConditional) {
    const nlohmann::json envelopeSchema = loadJsonFile(schemasDir() / "envelope.schema.json");
    // validationResult kind carrying a diff-shaped payload must NOT validate.
    EXPECT_FALSE(validatesAgainst(envelopeSchema, envelopeFor("validationResult", diffPayload())));
    // diffReport kind carrying a validation-shaped payload must NOT validate.
    EXPECT_FALSE(
        validatesAgainst(envelopeSchema, envelopeFor("diffReport", validationPayload())));
}
