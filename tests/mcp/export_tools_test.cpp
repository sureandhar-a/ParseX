#include <gtest/gtest.h>

#include <parsex/json_contract/schema_validate.hpp>

#include "dispatch.hpp"
#include "tool_registry.hpp"
#include "tools.hpp"

// Structure-only checks: exported DBC/CSV content is covered by library tests.
namespace {

std::string fixture(const std::string& name) {
    return std::string(PARSEX_FIXTURES_DIR) + "/" + name;
}

}  // namespace

TEST(ExportTool, EnvelopeConformsToSchemaAndInlinesContent) {
    for (const char* format : {"dbc", "csv"}) {
        nlohmann::json args = {{"path", fixture("system-4.2.arxml")}, {"format", format}};
        nlohmann::json packed;
        ASSERT_NO_THROW(packed = exportTool(args));
        const nlohmann::json& envelope = packed.at("structuredContent");
        EXPECT_EQ(envelope.at("kind"), "exportResult");
        const nlohmann::json& payload = envelope.at("payload");
        EXPECT_EQ(payload.at("format"), format);
        EXPECT_TRUE(payload.at("output").is_null());
        EXPECT_TRUE(payload.at("content").is_string());
        EXPECT_TRUE(payload.at("counts").at("frames").is_number_unsigned());
        EXPECT_TRUE(payload.at("counts").at("signals").is_number_unsigned());
        EXPECT_TRUE(payload.at("warnings").is_array());
        std::string error;
        EXPECT_TRUE(parsex::json_contract::validatesAgainstSchema(
            envelope, std::string(PARSEX_SCHEMAS_DIR) + "/envelope.schema.json", &error))
            << error;
    }
}

TEST(ExportTool, SummaryNamesCountsFormatAndPath) {
    const std::string path = fixture("system-4.2.arxml");
    nlohmann::json packed = exportTool({{"path", path}, {"format", "csv"}});
    const std::string summary = packed.at("summary").get<std::string>();
    EXPECT_EQ(summary.rfind("Exported ", 0), 0U) << summary;
    EXPECT_NE(summary.find(" as csv from " + path), std::string::npos) << summary;
}

TEST(ExportTool, MalformedInputThrowsForDispatch) {
    EXPECT_ANY_THROW(exportTool({{"path", fixture("malformed_unclosed.arxml")}, {"format", "dbc"}}));
}

TEST(ExportTool, DispatchRejectsBadArguments) {
    ToolRegistry registry = ToolRegistry::withSchemas();
    registry.setHandler("export_arxml", exportTool);
    const std::string path = fixture("system-4.2.arxml");
    for (const nlohmann::json& args : {nlohmann::json{{"path", path}},
                                       nlohmann::json{{"format", "dbc"}},
                                       nlohmann::json{{"path", path}, {"format", "xml"}},
                                       nlohmann::json{{"path", path}, {"format", "dbc"}, {"x", 1}}}) {
        nlohmann::json response =
            dispatchToolsCall(1, {{"name", "export_arxml"}, {"arguments", args}}, registry);
        ASSERT_TRUE(response.contains("error")) << args.dump();
        EXPECT_EQ(response["error"]["code"], -32602) << args.dump();
    }
}

TEST(ExportTool, DispatchSuccessCarriesStructuredContent) {
    ToolRegistry registry = ToolRegistry::withSchemas();
    registry.setHandler("export_arxml", exportTool);
    nlohmann::json response = dispatchToolsCall(
        2,
        {{"name", "export_arxml"},
         {"arguments", {{"path", fixture("system-4.2.arxml")}, {"format", "dbc"}}}},
        registry);
    ASSERT_TRUE(response.contains("result")) << response.dump();
    EXPECT_EQ(response["result"]["structuredContent"]["kind"], "exportResult");
}
