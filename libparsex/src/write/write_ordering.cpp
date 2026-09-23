#include <parsex/write/write_ordering.hpp>

#include <algorithm>

void setSortedAttributes(
    xmlNodePtr element,
    const std::vector<std::pair<std::string, std::string>>& attributes) {
    if (element == nullptr) {
        return;
    }
    std::vector<std::pair<std::string, std::string>> sorted = attributes;
    std::ranges::sort(sorted, {}, &std::pair<std::string, std::string>::first);
    for (const auto& [name, value] : sorted) {
        xmlNewProp(element, BAD_CAST name.c_str(), BAD_CAST value.c_str());
    }
}
