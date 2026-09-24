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

}  // namespace

TEST(WriteTool, PreviewReturnsWriteResultWithoutWriting) {
    nlohmann::json args = {{"path", writeFixture("schema_valid.arxml")},
                           {"outputPath", "/tmp/bridge-preview-test.arxml"}};
    std::filesystem::remove("/tmp/bridge-preview-test.arxml");
    nlohmann::json packed;
    ASSERT_NO_THROW(packed = writeTool(args));
    EXPECT_EQ(packed["structuredContent"]["kind"], "writeResult");
    EXPECT_EQ(packed["structuredContent"]["payload"]["applied"], false);
    EXPECT_FALSE(std::filesystem::exists("/tmp/bridge-preview-test.arxml"));
}
