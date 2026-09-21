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

// PAR-103: dangling vs resolving REFs are cleanly separated.
TEST(ValidateReferencesTest, DanglingRefIsReported) {
    constexpr const char* kDangling =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR>\n"
        "  <AR-PACKAGES>\n"
        "    <AR-PACKAGE>\n"
        "      <SHORT-NAME>Pkg</SHORT-NAME>\n"
        "      <ELEMENTS>\n"
        "        <CLUSTER><SHORT-NAME>Real</SHORT-NAME></CLUSTER>\n"
        "        <FRAME><SHORT-NAME>F1</SHORT-NAME>\n"
        "          <PDU-REF DEST=\"PDU\">/Pkg/Missing</PDU-REF>\n"
        "        </FRAME>\n"
        "      </ELEMENTS>\n"
        "    </AR-PACKAGE>\n"
        "  </AR-PACKAGES>\n"
        "</AUTOSAR>\n";
    const auto path = writeTemp("parsex_ref_dangling.arxml", kDangling);
    ParsedProject project;
    project.files.push_back(fileForPath(path));

    const ValidationResult result = Validator{}.validateReferences(project);
    ASSERT_EQ(result.errors.size(), 1U);
    EXPECT_EQ(result.errors.front().code, "ref.dangling");
    EXPECT_TRUE(result.errors.front().location.has_value());

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(ValidateReferencesTest, ResolvingRefProducesNoDanglingError) {
    constexpr const char* kValid =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR>\n"
        "  <AR-PACKAGES>\n"
        "    <AR-PACKAGE>\n"
        "      <SHORT-NAME>Pkg</SHORT-NAME>\n"
        "      <ELEMENTS>\n"
        "        <CLUSTER><SHORT-NAME>Real</SHORT-NAME></CLUSTER>\n"
        "        <FRAME><SHORT-NAME>F1</SHORT-NAME>\n"
        "          <PDU-REF DEST=\"PDU\">/Pkg/Real</PDU-REF>\n"
        "        </FRAME>\n"
        "      </ELEMENTS>\n"
        "    </AR-PACKAGE>\n"
        "  </AR-PACKAGES>\n"
        "</AUTOSAR>\n";
    const auto path = writeTemp("parsex_ref_resolving.arxml", kValid);
    ParsedProject project;
    project.files.push_back(fileForPath(path));

    const ValidationResult result = Validator{}.validateReferences(project);
    // Resolving-but-wrong-type is PAR-104's job — no dangling error here.
    // (DEST "PDU" vs actual "CLUSTER" must not leak into this category.)
    for (const auto& err : result.errors) {
        EXPECT_NE(err.code, "ref.dangling") << err.message;
    }

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

// PAR-104: DEST type-checking.
TEST(ValidateReferencesTest, CorrectDestTypeProducesNoError) {
    constexpr const char* kGood =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR>\n"
        "  <AR-PACKAGES>\n"
        "    <AR-PACKAGE>\n"
        "      <SHORT-NAME>Pkg</SHORT-NAME>\n"
        "      <ELEMENTS>\n"
        "        <CAN-FRAME><SHORT-NAME>F1</SHORT-NAME></CAN-FRAME>\n"
        "        <FRAME><SHORT-NAME>F2</SHORT-NAME>\n"
        "          <FRAME-REF DEST=\"FRAME\">/Pkg/F1</FRAME-REF>\n"
        "        </FRAME>\n"
        "      </ELEMENTS>\n"
        "    </AR-PACKAGE>\n"
        "  </AR-PACKAGES>\n"
        "</AUTOSAR>\n";
    const auto path = writeTemp("parsex_ref_goodtype.arxml", kGood);
    ParsedProject project;
    project.files.push_back(fileForPath(path));

    const ValidationResult result = Validator{}.validateReferences(project);
    EXPECT_TRUE(result.errors.empty());

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(ValidateReferencesTest, WrongDestTypeIsReported) {
    constexpr const char* kBad =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR>\n"
        "  <AR-PACKAGES>\n"
        "    <AR-PACKAGE>\n"
        "      <SHORT-NAME>Pkg</SHORT-NAME>\n"
        "      <ELEMENTS>\n"
        "        <I-SIGNAL><SHORT-NAME>S1</SHORT-NAME></I-SIGNAL>\n"
        "        <FRAME><SHORT-NAME>F1</SHORT-NAME>\n"
        "          <PDU-REF DEST=\"PDU-TRIGGERING\">/Pkg/S1</PDU-REF>\n"
        "        </FRAME>\n"
        "      </ELEMENTS>\n"
        "    </AR-PACKAGE>\n"
        "  </AR-PACKAGES>\n"
        "</AUTOSAR>\n";
    const auto path = writeTemp("parsex_ref_badtype.arxml", kBad);
    ParsedProject project;
    project.files.push_back(fileForPath(path));

    const ValidationResult result = Validator{}.validateReferences(project);
    ASSERT_EQ(result.errors.size(), 1U);
    EXPECT_EQ(result.errors.front().code, "ref.type_mismatch");
    EXPECT_EQ(result.errors.front().severity, Severity::Error);

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(ValidateReferencesTest, UnrecognizedDestIsWarningOnly) {
    constexpr const char* kUnknown =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR>\n"
        "  <AR-PACKAGES>\n"
        "    <AR-PACKAGE>\n"
        "      <SHORT-NAME>Pkg</SHORT-NAME>\n"
        "      <ELEMENTS>\n"
        "        <CLUSTER><SHORT-NAME>C1</SHORT-NAME></CLUSTER>\n"
        "        <FRAME><SHORT-NAME>F1</SHORT-NAME>\n"
        "          <FOO-REF DEST=\"SOME-FUTURE-CLASS\">/Pkg/C1</FOO-REF>\n"
        "        </FRAME>\n"
        "      </ELEMENTS>\n"
        "    </AR-PACKAGE>\n"
        "  </AR-PACKAGES>\n"
        "</AUTOSAR>\n";
    const auto path = writeTemp("parsex_ref_unknowndest.arxml", kUnknown);
    ParsedProject project;
    project.files.push_back(fileForPath(path));

    const ValidationResult result = Validator{}.validateReferences(project);
    ASSERT_EQ(result.errors.size(), 1U);
    EXPECT_EQ(result.errors.front().code, "ref.dest_unrecognized");
    EXPECT_EQ(result.errors.front().severity, Severity::Warning);
    EXPECT_FALSE(result.hasErrors());

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

// PAR-105: integrated pass — dangling, mismatch, and valid same-name case.
TEST(ValidateReferencesTest, IntegratedDanglingMismatchAndValidSameName) {
    // Valid project where "Foo" exists under two packages; the REF uses the
    // full path so a bare-name lookup bug would misresolve it.
    constexpr const char* kValidSameName =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR>\n"
        "  <AR-PACKAGES>\n"
        "    <AR-PACKAGE><SHORT-NAME>PackageA</SHORT-NAME><ELEMENTS>\n"
        "      <I-SIGNAL><SHORT-NAME>Foo</SHORT-NAME></I-SIGNAL>\n"
        "    </ELEMENTS></AR-PACKAGE>\n"
        "    <AR-PACKAGE><SHORT-NAME>PackageB</SHORT-NAME><ELEMENTS>\n"
        "      <I-SIGNAL><SHORT-NAME>Foo</SHORT-NAME></I-SIGNAL>\n"
        "      <FRAME><SHORT-NAME>F1</SHORT-NAME>\n"
        "        <SIGNAL-REF DEST=\"I-SIGNAL\">/PackageB/Foo</SIGNAL-REF>\n"
        "      </FRAME>\n"
        "    </ELEMENTS></AR-PACKAGE>\n"
        "  </AR-PACKAGES>\n"
        "</AUTOSAR>\n";
    const auto validPath = writeTemp("parsex_ref_integ_valid.arxml", kValidSameName);
    ParsedProject validProject;
    validProject.files.push_back(fileForPath(validPath));
    EXPECT_TRUE(Validator{}.validateReferences(validProject).errors.empty());
    std::error_code ignored;
    std::filesystem::remove(validPath, ignored);

    // One project mixing a dangling REF and a type-mismatched REF: each
    // category must appear exactly once, with no cross-contamination.
    constexpr const char* kMixed =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<AUTOSAR>\n"
        "  <AR-PACKAGES>\n"
        "    <AR-PACKAGE><SHORT-NAME>Pkg</SHORT-NAME><ELEMENTS>\n"
        "      <I-SIGNAL><SHORT-NAME>S1</SHORT-NAME></I-SIGNAL>\n"
        "      <FRAME><SHORT-NAME>F1</SHORT-NAME>\n"
        "        <PDU-REF DEST=\"PDU\">/Pkg/Nowhere</PDU-REF>\n"
        "        <SIGNAL-REF DEST=\"PDU-TRIGGERING\">/Pkg/S1</SIGNAL-REF>\n"
        "      </FRAME>\n"
        "    </ELEMENTS></AR-PACKAGE>\n"
        "  </AR-PACKAGES>\n"
        "</AUTOSAR>\n";
    const auto mixedPath = writeTemp("parsex_ref_integ_mixed.arxml", kMixed);
    ParsedProject mixedProject;
    mixedProject.files.push_back(fileForPath(mixedPath));
    const ValidationResult mixed = Validator{}.validateReferences(mixedProject);
    ASSERT_EQ(mixed.errors.size(), 2U);
    EXPECT_EQ(mixed.errors[0].code, "ref.dangling");
    EXPECT_EQ(mixed.errors[1].code, "ref.type_mismatch");
    std::filesystem::remove(mixedPath, ignored);
}
