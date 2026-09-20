#include <parsex/parser/secure_reader.hpp>

XmlReaderPtr openSecureReader(const std::filesystem::path& path) {
    xmlTextReaderPtr raw = xmlReaderForFile(path.c_str(), nullptr, kSecureReaderOptions);
    return XmlReaderPtr(raw);
}
