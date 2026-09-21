#include <parsex/parser/secure_reader.hpp>

#include <string>

XmlReaderPtr openSecureReader(const std::filesystem::path& path) {
    // libxml2 wants the filename as UTF-8 char: path::c_str() is wchar_t on
    // Windows, so copy the u8string() code units byte-for-byte (same dance as
    // the Loader and the schema tests).
    const auto utf8Path = path.u8string();
    const std::string narrowPath{utf8Path.begin(), utf8Path.end()};
    xmlTextReaderPtr raw = xmlReaderForFile(narrowPath.c_str(), nullptr, kSecureReaderOptions);
    return XmlReaderPtr(raw);
}
