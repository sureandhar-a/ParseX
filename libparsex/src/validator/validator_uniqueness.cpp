#include <parsex/validator/validator.hpp>

#include <map>
#include <vector>

#include <parsex/raw/raw_node.hpp>

namespace {

std::string trimText(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

const RawNode* findShortNameChild(const RawNode& node) {
    for (const auto& child : node.children) {
        if (child->tagName == "SHORT-NAME") {
            return child.get();
        }
    }
    return nullptr;
}

// Checks one parent scope (direct children only — never grandchildren) and
// recurses. parentPath is this node's own absolute path for messages.
void checkNode(const RawNode& node, const std::string& basePath,
               ValidationResult& out) {
    std::string nodePath = basePath;
    if (const RawNode* shortName = findShortNameChild(node)) {
        const std::string name = trimText(shortName->text);
        if (!name.empty()) {
            nodePath = basePath + "/" + name;
        }
    }

    std::map<std::string, std::vector<const RawNode*>> byName;
    for (const auto& child : node.children) {
        if (child->tagName == "SHORT-NAME") {
            continue;
        }
        const RawNode* childName = findShortNameChild(*child);
        if (childName == nullptr) {
            continue;
        }
        const std::string name = trimText(childName->text);
        if (!name.empty()) {
            byName[name].push_back(child.get());
        }
    }
    for (const auto& [name, members] : byName) {
        if (members.size() > 1) {
            ValidationError finding;
            finding.severity = Severity::Error;
            finding.code = "shortname.duplicate";
            std::string message = "duplicate SHORT-NAME '";
            message += name;
            message += "' under '";
            message += (nodePath.empty() ? "/" : nodePath);
            message += '\'';
            finding.message = std::move(message);
            // Point at the second occurrence; message names the first via path.
            finding.location = members.at(1)->span;
            finding.path = nodePath.empty() ? "/" : nodePath;
            out.errors.push_back(std::move(finding));
        }
    }

    for (const auto& child : node.children) {
        if (child->tagName == "SHORT-NAME") {
            continue;
        }
        checkNode(*child, nodePath, out);
    }
}

}  // namespace

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): stateless-by-design instance API — callers write Validator{}.validateUniqueness(...).
ValidationResult Validator::validateUniqueness(const ParsedProject& project) const {
    ValidationResult result;
    for (const auto& file : project.files) {
        if (file.rawDocument == nullptr) {
            continue;
        }
        checkNode(file.rawDocument->root, "", result);
    }
    return result;
}
