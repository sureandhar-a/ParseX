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

std::vector<std::string> sortedStrings(std::vector<std::string> names) {
    std::ranges::sort(names);
    return names;
}

std::vector<FramePduMapping> sortedFramePduMappings(std::vector<FramePduMapping> mappings) {
    std::ranges::sort(mappings, {}, &FramePduMapping::pduShortNameRef);
    return mappings;
}

std::vector<PduSignalMapping> sortedPduSignalMappings(
    std::vector<PduSignalMapping> mappings) {
    std::ranges::sort(mappings, {}, &PduSignalMapping::signalShortNameRef);
    return mappings;
}

std::vector<ValueTableEntry> sortedValueTableEntries(std::vector<ValueTableEntry> entries) {
    std::ranges::sort(entries, {}, &ValueTableEntry::value);
    return entries;
}
