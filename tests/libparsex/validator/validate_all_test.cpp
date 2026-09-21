// PAR-113: validateAll() runs and merges all four checks without dropping,
// duplicating, or short-circuiting any of them.
#include <gtest/gtest.h>

#include <parsex/parser/loader.hpp>
#include <parsex/schema/xml_schema_raii.hpp>
#include <parsex/validator/validator.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <set>

namespace {

std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    return path;
}

// Strict XSD: <Root> must contain exactly one <Child/> (no other content).
constexpr const char* kStrictXsd =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
    "  <xs:element name=\"Root\">\n"
    "    <xs:complexType>\n"
    "      <xs:sequence>\n"
    "        <xs:element name=\"Child\" type=\"xs:string\"/>\n"
    "      </xs:sequence>\n"
    "    </xs:complexType>\n"
    "  </xs:element>\n"
    "</xs:schema>\n";

XmlSchemaPtr loadSchemaFromFile(const std::filesystem::path& xsdPath) {
    XmlSchemaParserCtxtPtr ctxt(xmlSchemaNewParserCtxt(xsdPath.string().c_str()));
    EXPECT_NE(ctxt, nullptr);
    XmlSchemaPtr schema(xmlSchemaParse(ctxt.get()));
    EXPECT_NE(schema, nullptr);
    return schema;
}

ParsedFile fileForPath(const std::filesystem::path& path) {
    ParsedFile file;
    file.sourcePath = path;
    file.rawDocument = std::make_shared<RawDocument>(loadRawDocument(path));
    return file;
}

}  // namespace

TEST(ValidateAllTest, FullyValidProjectReturnsEmpty) {
    const auto xsdPath = writeTemp("parsex_all_strict.xsd", kStrictXsd);
    XmlSchemaPtr schema = loadSchemaFromFile(xsdPath);
    ASSERT_NE(schema, nullptr);

    // Schema-clean (<Root><Child/></Root>), no REFs, no duplicates, no CAN.
    const auto xmlPath =
        writeTemp("parsex_all_valid.xml", "<Root><Child>ok</Child></Root>\n");
    ParsedProject project;
    project.files.push_back(fileForPath(xmlPath));

    const ValidationResult result = Validator{}.validateAll(project, schema.get());
    EXPECT_TRUE(result.errors.empty());

    std::error_code ignored;
    std::filesystem::remove(xmlPath, ignored);
    std::filesystem::remove(xsdPath, ignored);
}

TEST(ValidateAllTest, OneProblemPerCategoryAllReportedNoShortCircuit) {
    const auto xsdPath = writeTemp("parsex_all_strict2.xsd", kStrictXsd);
    XmlSchemaPtr schema = loadSchemaFromFile(xsdPath);
    ASSERT_NE(schema, nullptr);

    // One file, four independent problems:
    // - schema: <Root> missing required <Child/>
    // - refs: dangling /Nowhere
    // - uniqueness: duplicate <ITEM><SHORT-NAME>Dup</...> siblings
    // - CAN: Frame length 40 (typed vector, independent of raw tree)
    constexpr const char* kBad =
        "<Root>\n"
        "  <FOO-REF DEST=\"BAR\">/Nowhere</FOO-REF>\n"
        "  <ITEM><SHORT-NAME>Dup</SHORT-NAME></ITEM>\n"
        "  <ITEM><SHORT-NAME>Dup</SHORT-NAME></ITEM>\n"
        "</Root>\n";
    const auto xmlPath = writeTemp("parsex_all_combined.xml", kBad);
    ParsedFile file = fileForPath(xmlPath);
    Frame badFrame;
    badFrame.common.shortName = "Bad";
    badFrame.length = 40;
    file.frames.push_back(badFrame);
    ParsedProject project;
    project.files.push_back(std::move(file));

    const ValidationResult result = Validator{}.validateAll(project, schema.get());
    std::set<std::string> codes;
    for (const auto& err : result.errors) {
        codes.insert(err.code);
    }
    EXPECT_NE(codes.find("schema.invalid"), codes.end()) << "missing schema error";
    EXPECT_NE(codes.find("ref.dangling"), codes.end()) << "missing dangling-ref error";
    EXPECT_NE(codes.find("shortname.duplicate"), codes.end()) << "missing uniqueness error";
    EXPECT_NE(codes.find("can.dlc_mismatch"), codes.end()) << "missing DLC error";
    EXPECT_EQ(result.errors.size(), 4U) << "expected exactly one finding per category";

    std::error_code ignored;
    std::filesystem::remove(xmlPath, ignored);
    std::filesystem::remove(xsdPath, ignored);
}
