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
    return tag.size() > 4 && tag.ends_with("-REF");
}

std::optional<std::string> destAttr(const RawNode& node) {
    for (const auto& attr : node.attributes) {
        if (attr.first == "DEST") {
            return attr.second;
        }
    }
    return std::nullopt;
}

struct RefSite {
    const RawNode* node = nullptr;
    std::string dest;
};

void collectRefs(const RawNode& node, std::vector<RefSite>& out) {
    if (isRefTag(node.tagName)) {
        if (const std::optional<std::string> dest = destAttr(node); dest.has_value()) {
            out.push_back({.node = &node, .dest = *dest});
        }
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

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): call sites pass (resolved tag, declared DEST) in documented order; the names differ by role.
bool tagMatchesDest(const std::string& actualTag, const std::string& dest) {
    const auto& table = destToTags();
    const auto found = table.find(dest);
    if (found == table.end()) {
        return true;  // unrecognized DEST handled as warning by caller
    }
    return std::ranges::find(found->second, actualTag) != found->second.end();
}

}  // namespace

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): stateless-by-design instance API — callers write Validator{}.buildReferencePathIndex(...).
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
        std::vector<RefSite> refs;
        collectRefs(file.rawDocument->root, refs);
        for (const RefSite& site : refs) {
            const RawNode* ref = site.node;
            const std::string& dest = site.dest;
            const std::string path = trimText(ref->text);
            if (path.empty()) {
                continue;  // malformed ref with no target: not file-resolvable
            }
            const auto target = index.find(path);
            if (target == index.end()) {
                ValidationError finding;
                finding.severity = Severity::Error;
                finding.code = "ref.dangling";
                std::string message = "dangling reference to '";
                message += path;
                message += '\'';
                finding.message = std::move(message);
                finding.location = ref->span;
                finding.path = path;
                result.errors.push_back(std::move(finding));
                continue;
            }
            // Step two: DEST type compare.
            const auto& table = destToTags();
            if (!table.contains(dest)) {
                // CAN-subset table only: unrecognized DEST on non-CAN refs
                // must not block validation of what ParseX understands.
                ValidationError finding;
                finding.severity = Severity::Warning;
                finding.code = "ref.dest_unrecognized";
                std::string message = "unrecognized DEST '";
                message += dest;
                message += "' for '";
                message += path;
                message += '\'';
                finding.message = std::move(message);
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
                std::string message = "reference to '";
                message += path;
                message += "' expects DEST '";
                message += dest;
                message += "' but resolves to <";
                message += actualTag;
                message += '>';
                finding.message = std::move(message);
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
