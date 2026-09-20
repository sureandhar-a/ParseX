// Full-fixture sweep: every Parser-era fixture through parseFile() in one
// CTest run. Under ENABLE_SANITIZERS (ASan+UBSan) this is the zero-findings
// gate — no crash, no hang, no UBSan report on any input; without sanitizers
// it still pins each fixture's expected outcome.

#include <gtest/gtest.h>

#include <parsex/parser/parse_error.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/parser/release_error.hpp>

#include <filesystem>
#include <string>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

}  // namespace

TEST(AllFixturesTest, ValidFixturesParseCleanly) {
    for (const char* name : {"parsefile_complete.arxml", "system-4.2.arxml",
                             "warnings_missing_fields.arxml",
                             "release_multiline.arxml",}) {
        EXPECT_NO_THROW(Parser{}.parseFile(fixture(name))) << name;
    }
}

TEST(AllFixturesTest, InvalidFixturesThrowWithoutCrashing) {
    EXPECT_THROW(Parser{}.parseFile(fixture("tiny_valid.arxml")), UnsupportedReleaseError);
    EXPECT_THROW(Parser{}.parseFile(fixture("loader_basic.arxml")), std::runtime_error);
    EXPECT_THROW(Parser{}.parseFile(fixture("loader_nested.arxml")), std::runtime_error);
    EXPECT_THROW(Parser{}.parseFile(fixture("malformed_unclosed.arxml")), ParseError);
    EXPECT_THROW(Parser{}.parseFile(fixture("xxe_attack.arxml")), ParseError);
    EXPECT_THROW(Parser{}.parseFile(fixture("xxe_ssrf.arxml")), ParseError);
}
