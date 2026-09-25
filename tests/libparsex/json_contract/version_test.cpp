// Unit test for the project-wide contract version constant (PAR-170).
#include <gtest/gtest.h>

#include <regex>
#include <string>

#include <parsex/json_contract/version.hpp>

TEST(ContractVersionTest, ParsesAsValidSemver) {
    static const std::regex kSemver(R"(^\d+\.\d+\.\d+$)");
    const std::string version{parsex::json_contract::kContractVersion};
    EXPECT_TRUE(std::regex_match(version, kSemver)) << version;
    EXPECT_EQ(version, "0.1.0");
}
