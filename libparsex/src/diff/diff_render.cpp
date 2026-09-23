#include <parsex/diff/diff_report.hpp>

#include <algorithm>
#include <sstream>
#include <string>

#include <parsex/json_contract/envelope.hpp>

// Human-readable text rendering (PAR-132).
// Groups entries into Added/Removed/Moved/Modified sections in fixed order,
// skipping empty sections. Plain text, consistent indentation, grep-friendly
// — meant for terminals and review comments, not fancy tables.

namespace {

void renderFieldLines(std::ostringstream& text, const DiffEntry& entry) {
    for (const auto& field : entry.fieldDiffs) {
        text << "    " << field.fieldName << ": " << field.oldValue << " -> " << field.newValue
             << "\n";
    }
}

void renderEntryLines(std::ostringstream& text, const DiffEntry& entry) {
    switch (entry.kind) {
        case DiffKind::Added:
            text << "  " << entry.elementType << " " << entry.newPath << "\n";
            break;
        case DiffKind::Removed:
            text << "  " << entry.elementType << " " << entry.oldPath << "\n";
            break;
        case DiffKind::Moved:
            text << "  " << entry.elementType << " " << entry.oldPath << " -> " << entry.newPath
                 << "\n";
            renderFieldLines(text, entry);
            break;
        case DiffKind::Modified:
            text << "  " << entry.elementType << " " << entry.newPath << "\n";
            renderFieldLines(text, entry);
            break;
    }
}

bool hasKind(const std::vector<DiffEntry>& entries, DiffKind kind) {
    return std::ranges::any_of(
        entries, [kind](const DiffEntry& entry) { return entry.kind == kind; });
}

}  // namespace

std::string DiffReport::toText() const {
    if (entries.empty() && diagnostics.empty()) {
        return "No differences.\n";
    }
    std::ostringstream text;
    bool firstSection = true;
    const auto emitSection = [&](const char* title, DiffKind kind) {
        if (!hasKind(entries, kind)) {
            return;
        }
        if (!firstSection) {
            text << "\n";
        }
        firstSection = false;
        text << title << ":\n";
        for (const auto& entry : entries) {
            if (entry.kind == kind) {
                renderEntryLines(text, entry);
            }
        }
    };
    emitSection("Added", DiffKind::Added);
    emitSection("Removed", DiffKind::Removed);
    emitSection("Moved", DiffKind::Moved);
    emitSection("Modified", DiffKind::Modified);

    if (!diagnostics.empty()) {
        if (!firstSection) {
            text << "\n";
        }
        text << "Diagnostics:\n";
        for (const auto& diag : diagnostics) {
            text << "  [" << diag.elementType << "] " << diag.message << "\n";
        }
    }
    return text.str();
}

namespace {

std::string kindToJson(DiffKind kind) {
    switch (kind) {
        case DiffKind::Added:
            return "added";
        case DiffKind::Removed:
            return "removed";
        case DiffKind::Moved:
            return "moved";
        case DiffKind::Modified:
            return "modified";
    }
    return "unknown";
}

DiffKind kindFromJson(const std::string& kindName) {
    if (kindName == "added") {
        return DiffKind::Added;
    }
    if (kindName == "removed") {
        return DiffKind::Removed;
    }
    if (kindName == "moved") {
        return DiffKind::Moved;
    }
    return DiffKind::Modified;
}

}  // namespace

nlohmann::json DiffReport::toJson() const {
    nlohmann::json payload;
    payload["entries"] = nlohmann::json::array();
    for (const auto& entry : entries) {
        nlohmann::json entryJson;
        entryJson["kind"] = kindToJson(entry.kind);
        entryJson["elementType"] = entry.elementType;
        // Omit-optional per CONVENTIONS.md (PAR-174 migration): absent paths
        // travel as missing keys, not empty strings as pre-contract code did.
        if (!entry.oldPath.empty()) {
            entryJson["oldPath"] = entry.oldPath;
        }
        if (!entry.newPath.empty()) {
            entryJson["newPath"] = entry.newPath;
        }
        entryJson["fieldDiffs"] = nlohmann::json::array();
        for (const auto& field : entry.fieldDiffs) {
            nlohmann::json fieldJson;
            fieldJson["field"] = field.fieldName;
            fieldJson["oldValue"] = field.oldValue;
            fieldJson["newValue"] = field.newValue;
            entryJson["fieldDiffs"].push_back(std::move(fieldJson));
        }
        payload["entries"].push_back(std::move(entryJson));
    }
    payload["diagnostics"] = nlohmann::json::array();
    for (const auto& diag : diagnostics) {
        nlohmann::json diagJson;
        diagJson["elementType"] = diag.elementType;
        diagJson["message"] = diag.message;
        payload["diagnostics"].push_back(std::move(diagJson));
    }
    // Envelope via the shared helper (PAR-176): single construction site.
    return parsex::json_contract::wrapEnvelope("diffReport", std::move(payload));
}

DiffReport DiffReport::fromJson(const nlohmann::json& jsonDoc) {
    // Accept the full envelope (as produced by toJson) or a bare payload.
    const nlohmann::json& payload =
        (jsonDoc.is_object() && jsonDoc.contains("payload")) ? jsonDoc.at("payload") : jsonDoc;
    DiffReport report;
    for (const auto& entryJson : payload.at("entries")) {
        DiffEntry entry;
        entry.kind = kindFromJson(entryJson.at("kind").get<std::string>());
        entry.elementType = entryJson.at("elementType").get<std::string>();
        entry.oldPath = entryJson.value("oldPath", "");
        entry.newPath = entryJson.value("newPath", "");
        for (const auto& fieldJson : entryJson.at("fieldDiffs")) {
            entry.fieldDiffs.push_back(
                {.fieldName = fieldJson.at("field").get<std::string>(),
                 .oldValue = fieldJson.at("oldValue").get<std::string>(),
                 .newValue = fieldJson.at("newValue").get<std::string>()});
        }
        report.entries.push_back(std::move(entry));
    }
    if (payload.contains("diagnostics")) {
        for (const auto& diagJson : payload.at("diagnostics")) {
            report.diagnostics.push_back(
                {.message = diagJson.at("message").get<std::string>(),
                 .elementType = diagJson.at("elementType").get<std::string>()});
        }
    }
    return report;
}
