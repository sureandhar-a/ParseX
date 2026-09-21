#include <parsex/parser/release_detector.hpp>

#include <cstddef>
#include <map>

namespace {

bool isSchemaLocationSpace(char letter) {
    return letter == ' ' || letter == '\t' || letter == '\r' || letter == '\n';
}

std::size_t skipSpaces(const std::string& value, std::size_t pos) {
    while (pos < value.size() && isSchemaLocationSpace(value.at(pos))) {
        ++pos;
    }
    return pos;
}

std::size_t tokenEnd(const std::string& value, std::size_t pos) {
    while (pos < value.size() && !isSchemaLocationSpace(value.at(pos))) {
        ++pos;
    }
    return pos;
}

}  // namespace

std::optional<std::string> detectSchemaFilename(const RawDocument& document) {
    const std::string* location = nullptr;
    for (const auto& attr : document.root.attributes) {
        if (attr.first == "xsi:schemaLocation") {
            location = &attr.second;
            break;
        }
    }
    if (location == nullptr) {
        return std::nullopt;
    }
    // Second whitespace-separated token is the filename; anything else
    // (empty value, lone namespace, no filename) means "unknown".
    const std::size_t firstEnd = tokenEnd(*location, skipSpaces(*location, 0));
    const std::size_t secondStart = skipSpaces(*location, firstEnd);
    if (secondStart >= location->size()) {
        return std::nullopt;
    }
    return location->substr(secondStart, tokenEnd(*location, secondStart) - secondStart);
}

const std::map<std::string, std::string>& schemaFilenameTable() {
    // Keys are AUTOSAR's distribution filenames (see scripts/download_schemas.py);
    // values are the Schema Registry's release keys (resources/schemas/<release>.xsd).
    static const std::map<std::string, std::string> table{
        {"AUTOSAR_4-2-2.xsd", "4.2.2"},
        {"AUTOSAR_4-3-0.xsd", "4.3.0"},
        {"AUTOSAR_00044.xsd", "4.3.1"},
        {"AUTOSAR_00046.xsd", "4.4.0"},
        {"AUTOSAR_00048.xsd", "R19-11"},
        {"AUTOSAR_00049.xsd", "R20-11"},
        {"AUTOSAR_00050.xsd", "R21-11"},
    };
    return table;
}

std::string resolveRelease(const std::string& schemaFilename) {
    const auto found = schemaFilenameTable().find(schemaFilename);
    if (found == schemaFilenameTable().end()) {
        throw UnsupportedReleaseError(schemaFilename);
    }
    return found->second;
}
