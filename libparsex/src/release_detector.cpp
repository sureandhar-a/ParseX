#include <parsex/parser/release_detector.hpp>

#include <cstddef>

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
