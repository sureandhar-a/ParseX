#include <gtest/gtest.h>

#include "tools.hpp"

namespace {

std::string writeFixture(const std::string& name) {
#ifdef PARSEX_FIXTURES_DIR
    return std::string(PARSEX_FIXTURES_DIR) + "/" + name;
#else
    return "tests/fixtures/" + name;
#endif
}

std::string tempOutput(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

}  // namespace

TEST(WriteTool, PreviewReturnsWriteResultWithoutWriting) {
    const std::string previewOut = tempOutput("bridge-preview-test.arxml");
    nlohmann::json args = {{"path", writeFixture("schema_valid.arxml")},
                           {"outputPath", previewOut}};
    std::filesystem::remove(previewOut);
    nlohmann::json packed;
    ASSERT_NO_THROW(packed = writeTool(args));
    EXPECT_EQ(packed["structuredContent"]["kind"], "writeResult");
    EXPECT_EQ(packed["structuredContent"]["payload"]["applied"], false);
    EXPECT_FALSE(std::filesystem::exists(previewOut));
    EXPECT_NE(packed["summary"].get<std::string>().find("DRY RUN"), std::string::npos);
}

TEST(WriteTool, ApplyWritesFile) {
    const std::string out = tempOutput("bridge-apply-test.arxml");
    std::filesystem::remove(out);
    nlohmann::json args = {
        {"path", writeFixture("schema_valid.arxml")}, {"outputPath", out}, {"apply", true}};
    nlohmann::json packed;
    ASSERT_NO_THROW(packed = writeTool(args));
    EXPECT_EQ(packed["structuredContent"]["payload"]["applied"], true);
    EXPECT_TRUE(std::filesystem::exists(out));
    std::filesystem::remove(out);
}
