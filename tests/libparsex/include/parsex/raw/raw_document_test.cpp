#include <gtest/gtest.h>
#include <libxml/tree.h>
#include <parsex/raw/raw_document.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

std::string makeTemporaryXmlFile() {
    const auto path = std::filesystem::temp_directory_path() / "parsex_raw_document_test.xml";
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to create temp XML file");
    }

    out << R"(<?xml version="1.0"?>
           <DataBuffer size="512">
               <status>Pending</status>
           </DataBuffer>)";

    if (!out) {
        throw std::runtime_error("failed to write temp XML file");
    }

    return path.string();
}

RawDocument makeRawDocumentFromFileAndThrow(const std::string& path) {
    xmlDocPtr rawDoc = xmlReadFile(path.c_str(), nullptr, 0);
    if (rawDoc == nullptr) {
        throw std::runtime_error("xmlReadFile failed");
    }

    RawDocument doc{XmlDocPtr(rawDoc, XmlDocDeleter())};
    EXPECT_NE(doc.nativeHandle(), nullptr);

    throw std::runtime_error("forced failure");
}

}  // namespace

TEST(RawDocumentTest, NewRawDocument) {
    const auto xmlPath = makeTemporaryXmlFile();

    try {
        makeRawDocumentFromFileAndThrow(xmlPath);
        FAIL() << "Expected std::runtime_error to be thrown";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ("forced failure", e.what());
    }

    std::filesystem::remove(xmlPath);
}