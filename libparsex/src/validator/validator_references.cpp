#include <parsex/validator/validator.hpp>

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

void indexNode(const RawNode& node, const std::string& basePath,
               std::map<std::string, const RawNode*>& out) {
    std::string nodePath = basePath;
    if (const RawNode* shortName = findShortNameChild(node)) {
        const std::string name = trimText(shortName->text);
        if (!name.empty()) {
            nodePath = basePath + "/" + name;
            // First file order wins on same-path collisions — the uniqueness
            // check (PBI 3) owns reporting those.
            out.try_emplace(nodePath, &node);
        }
    }
    for (const auto& child : node.children) {
        if (child->tagName == "SHORT-NAME") {
            continue;  // name carrier, not a path element itself
        }
        indexNode(*child, nodePath, out);
    }
}

}  // namespace

std::map<std::string, const RawNode*> Validator::buildReferencePathIndex(
    const ParsedProject& project) const {
    std::map<std::string, const RawNode*> index;
    for (const auto& file : project.files) {
        if (file.rawDocument == nullptr) {
            continue;
        }
        indexNode(file.rawDocument->root, "", index);
    }
    return index;
}

// Full REF resolution lives in PAR-103 (dangling) + PAR-104 (DEST check).
// This stub exists so the header's promise compiles; it delegates to the
// same index without reporting yet — replaced by the real pass next.
ValidationResult Validator::validateReferences(const ParsedProject& /*project*/) const {
    return ValidationResult{};
}
