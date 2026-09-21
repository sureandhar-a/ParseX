// PAR-106/107: per-parent short-name uniqueness (positive + negative cases).
#include <gtest/gtest.h>

#include <parsex/parser/loader.hpp>
#include <parsex/validator/validator.hpp>

#include <filesystem>
#include <fstream>
#include <memory>

namespace {

std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    return path;
}

ParsedFile fileForPath(const std::filesystem::path& path) {
    ParsedFile file;
    file.sourcePath = path;
    file.rawDocument = std::make_shared<RawDocument>(loadRawDocument(path));
    return file;
}

constexpr const char* kDuplicateSiblings =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<AUTOSAR><AR-PACKAGES><AR-PACKAGE>\n"
    "  <SHORT-NAME>Pkg</SHORT-NAME>\n"
    "  <ELEMENTS>\n"
    "    <CLUSTER><SHORT-NAME>Dup</SHORT-NAME></CLUSTER>\n"
    "    <CLUSTER><SHORT-NAME>Dup</SHORT-NAME></CLUSTER>\n"
    "  </ELEMENTS>\n"
    "</AR-PACKAGE></AR-PACKAGES></AUTOSAR>\n";

constexpr const char* kSameNameDifferentPackages =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<AUTOSAR><AR-PACKAGES>\n"
    "  <AR-PACKAGE><SHORT-NAME>PackageA</SHORT-NAME><ELEMENTS>\n"
    "    <CLUSTER><SHORT-NAME>Foo</SHORT-NAME></CLUSTER>\n"
    "  </ELEMENTS></AR-PACKAGE>\n"
    "  <AR-PACKAGE><SHORT-NAME>PackageB</SHORT-NAME><ELEMENTS>\n"
    "    <CLUSTER><SHORT-NAME>Foo</SHORT-NAME></CLUSTER>\n"
    "  </ELEMENTS></AR-PACKAGE>\n"
    "</AR-PACKAGES></AUTOSAR>\n";

}  // namespace

TEST(ValidateUniquenessTest, DuplicateSiblingsAreFlagged) {
    const auto path = writeTemp("parsex_unique_dup.arxml", kDuplicateSiblings);
    ParsedProject project;
    project.files.push_back(fileForPath(path));

    const ValidationResult result = Validator{}.validateUniqueness(project);
    ASSERT_EQ(result.errors.size(), 1U);
    EXPECT_EQ(result.errors.front().code, "shortname.duplicate");
    EXPECT_TRUE(result.errors.front().location.has_value());

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(ValidateUniquenessTest, SameNameAcrossPackagesIsAllowed) {
    const auto path = writeTemp("parsex_unique_cross.arxml", kSameNameDifferentPackages);
    ParsedProject project;
    project.files.push_back(fileForPath(path));

    const ValidationResult result = Validator{}.validateUniqueness(project);
    EXPECT_TRUE(result.errors.empty());

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
