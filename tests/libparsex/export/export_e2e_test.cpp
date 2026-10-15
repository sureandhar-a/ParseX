// End to end: parse system-4.2.arxml (cantools' own fixture), export, and pin
// the lines scripts/check_dbc_against_cantools.py verified against cantools 44.
#include <algorithm>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include <parsex/export/can_db.hpp>
#include <parsex/export/csv_writer.hpp>
#include <parsex/export/dbc_writer.hpp>
#include <parsex/parser/parser.hpp>

namespace {

ExportDatabase systemFourTwo() {
    return buildExportDatabase(
        Parser{}.parseFile(std::filesystem::path(PARSEX_FIXTURE_DIR) / "system-4.2.arxml"));
}

}  // namespace

TEST(ExportE2E, DbcMatchesCantoolsVerifiedLines) {
    const std::string dbc = toDbc(systemFourTwo());
    for (const char* line : {
             "BU_: DJ Dancer Guard\n",
             "BO_ 5 Message1: 9 DJ\n",
             " SG_ signal1 : 36|3@0+ (5,0) [0|20] \"m\" Dancer\n",
             " SG_ signal6 : 32|1@1+ (0.1,0) [0|0.1] \"wp\" Dancer\n",
             "BO_ 2147483654 Message2: 7 Dancer\n",
             " SG_ signal2 : 18|11@1- (1,0) [0|0] \"\" DJ\n",
             "BA_ \"VFrameFormat\" BO_ 2147483654 15;\n",
             "VAL_ 2147483654 signal4 1 \"one\" 2 \"two\" ;\n",
         }) {
        EXPECT_NE(dbc.find(line), std::string::npos) << line;
    }
}

TEST(ExportE2E, WarningsNameOnlyUnsupportedPdus) {
    const ExportDatabase db = systemFourTwo();
    EXPECT_EQ(db.frames.size(), 8U);
    EXPECT_EQ(db.warnings.size(), 4U);  // multiplexed, secured, container, NM PDUs
    for (const Warning& warning : db.warnings) {
        EXPECT_NE(warning.message.find("is not exported"), std::string::npos) << warning.message;
    }
}

TEST(ExportE2E, CsvHasOneRowPerSignalOrEmptyFrame) {
    const std::string csv = toCsv(systemFourTwo());
    // header + 11 signals + 5 frames without exported signals
    EXPECT_EQ(std::ranges::count(csv, '\n'), 1 + 11 + 5);
    EXPECT_NE(csv.find("Message1,0x5,false,true,9,DJ,signal1,36,3,big_endian,false,5,0,0,20,m,"),
              std::string::npos);
}
