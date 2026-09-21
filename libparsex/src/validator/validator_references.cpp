#include <parsex/validator/validator.hpp>

#include <algorithm>
#include <optional>
#include <ranges>
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

// DEST -> acceptable XML tags, scoped to the CAN-stack subset ParseX models.
// Aliases included (FRAME/CAN-FRAME, PDU/I-SIGNAL-I-PDU, ...) per the
// Protocol Model Builder's accepted tags — grep the XSDs before extending.
const std::map<std::string, std::vector<std::string>>& destToTags() {
    static const std::map<std::string, std::vector<std::string>> kTable = {
        {"FRAME", {"FRAME", "CAN-FRAME"}},
        {"PDU", {"PDU", "I-SIGNAL-I-PDU"}},
        {"PDU-TRIGGERING", {"PDU-TRIGGERING"}},
        {"I-SIGNAL", {"I-SIGNAL"}},
        {"I-SIGNAL-GROUP", {"I-SIGNAL-GROUP", "SIGNAL-GROUP"}},
        {"SYSTEM-SIGNAL", {"SYSTEM-SIGNAL"}},
        {"SIGNAL-GROUP", {"SIGNAL-GROUP", "I-SIGNAL-GROUP"}},
        {"ECU-INSTANCE", {"ECU-INSTANCE"}},
        {"PHYSICAL-CHANNEL", {"PHYSICAL-CHANNEL", "CAN-PHYSICAL-CHANNEL"}},
    };
    return kTable;
}

bool tagMatchesDest(const std::string& actualTag, const std::string& dest) {
    const auto& table = destToTags();
    const auto it = table.find(dest);
    if (it == table.end()) {
        return true;  // unrecognized DEST handled as warning by caller
    }
    return std::ranges::find(it->second, actualTag) != it->second.end();
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

// PAR-103: path lookup + dangling detection. PAR-104: DEST type-check on top
// (two-step autosar-data algorithm: lookup, then type compare).
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
            const auto target = index.find(path);
            if (target == index.end()) {
                ValidationError finding;
                finding.severity = Severity::Error;
                finding.code = "ref.dangling";
                finding.message = "dangling reference to '" + path + "'";
                finding.location = ref->span;
                finding.path = path;
                result.errors.push_back(std::move(finding));
                continue;
            }
            // Step two: DEST type compare.
            const std::string dest = destAttr(*ref).value();
            const auto& table = destToTags();
            if (table.find(dest) == table.end()) {
                // CAN-subset table only: unrecognized DEST on non-CAN refs
                // must not block validation of what ParseX understands.
                ValidationError finding;
                finding.severity = Severity::Warning;
                finding.code = "ref.dest_unrecognized";
                finding.message = "unrecognized DEST '" + dest + "' for '" + path + "'";
                finding.location = ref->span;
                finding.path = path;
                finding.expectedType = dest;
                result.errors.push_back(std::move(finding));
                continue;
            }
            const std::string actualTag = target->second->tagName;
            if (!tagMatchesDest(actualTag, dest)) {
                ValidationError finding;
                finding.severity = Severity::Error;
                finding.code = "ref.type_mismatch";
                finding.message = "reference to '" + path + "' expects DEST '" + dest +
                                  "' but resolves to <" + actualTag + ">";
                finding.location = ref->span;
                finding.path = path;
                finding.expectedType = dest;
                finding.actualType = actualTag;
                result.errors.push_back(std::move(finding));
            }
        }
    }
    return result;
}
