#include <parsex/validator/validator.hpp>

#include <optional>
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

bool isRefTag(const std::string& tag) {
    // AUTOSAR references are XXX-REF (TRANSMITTER-REF, PDU-REF, ...).
    // Bare "REF" alone is not a reference element.
    return tag.size() > 4 && tag.compare(tag.size() - 4, 4, "-REF") == 0;
}

std::optional<std::string> destAttr(const RawNode& node) {
    for (const auto& attr : node.attributes) {
        if (attr.first == "DEST") {
            return attr.second;
        }
    }
    return std::nullopt;
}

void collectRefs(const RawNode& node, std::vector<const RawNode*>& out) {
    if (isRefTag(node.tagName) && destAttr(node).has_value()) {
        out.push_back(&node);
    }
    for (const auto& child : node.children) {
        collectRefs(*child, out);
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

// PAR-103: path lookup + dangling detection. DEST type-checking is PAR-104's
// job — a resolving-but-wrong-type REF must NOT be flagged as dangling here.
ValidationResult Validator::validateReferences(const ParsedProject& project) const {
    ValidationResult result;
    const auto index = buildReferencePathIndex(project);
    for (const auto& file : project.files) {
        if (file.rawDocument == nullptr) {
            continue;
        }
        std::vector<const RawNode*> refs;
        collectRefs(file.rawDocument->root, refs);
        for (const RawNode* ref : refs) {
            const std::string path = trimText(ref->text);
            if (path.empty()) {
                continue;  // malformed ref with no target: not file-resolvable
            }
            if (index.find(path) != index.end()) {
                continue;  // resolves — type-check belongs to PAR-104
            }
            ValidationError finding;
            finding.severity = Severity::Error;
            finding.code = "ref.dangling";
            finding.message = "dangling reference to '" + path + "'";
            finding.location = ref->span;
            finding.path = path;
            result.errors.push_back(std::move(finding));
        }
    }
    return result;
}
