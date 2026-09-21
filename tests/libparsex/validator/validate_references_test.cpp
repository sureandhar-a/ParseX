// PAR-102: short-name package-path index over the raw tree.
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

constexpr const char* kNested =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<AUTOSAR>\n"
    "  <AR-PACKAGES>\n"
    "    <AR-PACKAGE>\n"
    "      <SHORT-NAME>PackageA</SHORT-NAME>\n"
    "      <AR-PACKAGES>\n"
    "        <AR-PACKAGE>\n"
    "          <SHORT-NAME>SubPackage</SHORT-NAME>\n"
    "          <ELEMENTS>\n"
    "            <CLUSTER>\n"
    "              <SHORT-NAME>ElementName</SHORT-NAME>\n"
    "            </CLUSTER>\n"
    "          </ELEMENTS>\n"
    "        </AR-PACKAGE>\n"
    "      </AR-PACKAGES>\n"
    "    </AR-PACKAGE>\n"
    "  </AR-PACKAGES>\n"
    "</AUTOSAR>\n";

}  // namespace

TEST(ReferencePathIndexTest, NestedPackagesProduceAbsolutePath) {
    const auto path = writeTemp("parsex_ref_nested.arxml", kNested);
    ParsedProject project;
    project.files.push_back(fileForPath(path));

    const auto index = Validator{}.buildReferencePathIndex(project);
    EXPECT_NE(index.find("/PackageA"), index.end());
    EXPECT_NE(index.find("/PackageA/SubPackage"), index.end());
    EXPECT_NE(index.find("/PackageA/SubPackage/ElementName"), index.end());

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(ReferencePathIndexTest, SameNameUnderDifferentPackagesIsDistinct) {
    constexpr const char* kTwoPkgs =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR>\n"
        "  <AR-PACKAGES>\n"
        "    <AR-PACKAGE>\n"
        "      <SHORT-NAME>PackageA</SHORT-NAME>\n"
        "      <ELEMENTS><CLUSTER><SHORT-NAME>Foo</SHORT-NAME></CLUSTER></ELEMENTS>\n"
        "    </AR-PACKAGE>\n"
        "    <AR-PACKAGE>\n"
        "      <SHORT-NAME>PackageB</SHORT-NAME>\n"
        "      <ELEMENTS><CLUSTER><SHORT-NAME>Foo</SHORT-NAME></CLUSTER></ELEMENTS>\n"
        "    </AR-PACKAGE>\n"
        "  </AR-PACKAGES>\n"
        "</AUTOSAR>\n";
    const auto path = writeTemp("parsex_ref_twopkgs.arxml", kTwoPkgs);
    ParsedProject project;
    project.files.push_back(fileForPath(path));

    const auto index = Validator{}.buildReferencePathIndex(project);
    const auto a = index.find("/PackageA/Foo");
    const auto b = index.find("/PackageB/Foo");
    ASSERT_NE(a, index.end());
    ASSERT_NE(b, index.end());
    EXPECT_NE(a->second, b->second);

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
