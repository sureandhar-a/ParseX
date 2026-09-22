#include <parsex/diff/diff_report.hpp>

#include <sstream>

// Human-readable text rendering (PAR-132).
// Groups entries into Added/Removed/Moved/Modified sections in fixed order,
// skipping empty sections. Plain text, consistent indentation, grep-friendly
// — meant for terminals and review comments, not fancy tables.

std::string DiffReport::toText() const {
    if (entries.empty() && diagnostics.empty()) {
        return "No differences.\n";
    }
    std::ostringstream oss;
    bool firstSection = true;
    const auto emitSection = [&](const char* title, DiffKind kind) {
        bool any = false;
        for (const auto& e : entries) {
            if (e.kind == kind) {
                any = true;
                break;
            }
        }
        if (!any) {
            return;
        }
        if (!firstSection) {
            oss << "\n";
        }
        firstSection = false;
        oss << title << ":\n";
        for (const auto& e : entries) {
            if (e.kind != kind) {
                continue;
            }
            switch (kind) {
                case DiffKind::Added:
                    oss << "  " << e.elementType << " " << e.newPath << "\n";
                    break;
                case DiffKind::Removed:
                    oss << "  " << e.elementType << " " << e.oldPath << "\n";
                    break;
                case DiffKind::Moved:
                    oss << "  " << e.elementType << " " << e.oldPath << " -> " << e.newPath
                        << "\n";
                    for (const auto& f : e.fieldDiffs) {
                        oss << "    " << f.fieldName << ": " << f.oldValue << " -> " << f.newValue
                            << "\n";
                    }
                    break;
                case DiffKind::Modified:
                    oss << "  " << e.elementType << " " << e.newPath << "\n";
                    for (const auto& f : e.fieldDiffs) {
                        oss << "    " << f.fieldName << ": " << f.oldValue << " -> " << f.newValue
                            << "\n";
                    }
                    break;
            }
        }
    };
    emitSection("Added", DiffKind::Added);
    emitSection("Removed", DiffKind::Removed);
    emitSection("Moved", DiffKind::Moved);
    emitSection("Modified", DiffKind::Modified);

    if (!diagnostics.empty()) {
        if (!firstSection) {
            oss << "\n";
        }
        oss << "Diagnostics:\n";
        for (const auto& d : diagnostics) {
            oss << "  [" << d.elementType << "] " << d.message << "\n";
        }
    }
    return oss.str();
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

DiffKind kindFromJson(const std::string& s) {
    if (s == "added") {
        return DiffKind::Added;
    }
    if (s == "removed") {
        return DiffKind::Removed;
    }
    if (s == "moved") {
        return DiffKind::Moved;
    }
    return DiffKind::Modified;
}

}  // namespace

nlohmann::json DiffReport::toJson() const {
    nlohmann::json j;
    j["entries"] = nlohmann::json::array();
    for (const auto& e : entries) {
        nlohmann::json entry;
        entry["kind"] = kindToJson(e.kind);
        entry["elementType"] = e.elementType;
        entry["oldPath"] = e.oldPath;
        entry["newPath"] = e.newPath;
        entry["fieldDiffs"] = nlohmann::json::array();
        for (const auto& f : e.fieldDiffs) {
            nlohmann::json field;
            field["field"] = f.fieldName;
            field["oldValue"] = f.oldValue;
            field["newValue"] = f.newValue;
            entry["fieldDiffs"].push_back(std::move(field));
        }
        j["entries"].push_back(std::move(entry));
    }
    j["diagnostics"] = nlohmann::json::array();
    for (const auto& d : diagnostics) {
        nlohmann::json diag;
        diag["elementType"] = d.elementType;
        diag["message"] = d.message;
        j["diagnostics"].push_back(std::move(diag));
    }
    return j;
}

DiffReport DiffReport::fromJson(const nlohmann::json& j) {
    DiffReport report;
    for (const auto& entry : j.at("entries")) {
        DiffEntry e;
        e.kind = kindFromJson(entry.at("kind").get<std::string>());
        e.elementType = entry.at("elementType").get<std::string>();
        e.oldPath = entry.at("oldPath").get<std::string>();
        e.newPath = entry.at("newPath").get<std::string>();
        for (const auto& field : entry.at("fieldDiffs")) {
            e.fieldDiffs.push_back({.fieldName = field.at("field").get<std::string>(),
                                    .oldValue = field.at("oldValue").get<std::string>(),
                                    .newValue = field.at("newValue").get<std::string>()});
        }
        report.entries.push_back(std::move(e));
    }
    if (j.contains("diagnostics")) {
        for (const auto& diag : j.at("diagnostics")) {
            report.diagnostics.push_back({.message = diag.at("message").get<std::string>(),
                                           .elementType = diag.at("elementType").get<std::string>()});
        }
    }
    return report;
}
