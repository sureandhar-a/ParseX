#pragma once

#include <map>
#include <string>
#include <vector>

#include <parsex/diff/diff_report.hpp>
#include <parsex/model/parsed_project.hpp>

// Path-based matching core (PAR-122).
//
// Path construction reuses the Validator reference-resolution convention
// (PAR-94): '/'-separated absolute short-name paths ("/Pkg/Sub/Element").
// Typed domain objects carry only CommonFields::shortName (package hierarchy
// is not retained by the Model Builder), so the path here is "/" +
// shortName — the same leading-slash absolute form, degenerate to one
// segment. First-wins on duplicate paths, mirroring the Validator index.

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): placeholder for future per-type path customization.
[[nodiscard]] inline std::string diffPathForShortName(const std::string& shortName) {
    if (shortName.empty()) {
        return "";
    }
    return "/" + shortName;
}

template <typename T>
[[nodiscard]] std::map<std::string, const T*> indexByPath(const std::vector<T>& elements) {
    std::map<std::string, const T*> index;
    for (const auto& element : elements) {
        const std::string path = diffPathForShortName(element.common.shortName);
        if (path.empty()) {
            continue;
        }
        index.try_emplace(path, &element);
    }
    return index;
}

template <typename T>
[[nodiscard]] DiffReport matchByPath(const std::map<std::string, const T*>& oldIndex,
                                     const std::map<std::string, const T*>& newIndex,
                                     const std::string& elementTypeName) {
    DiffReport report;
    for (const auto& [path, oldPtr] : oldIndex) {
        (void)oldPtr;
        if (!newIndex.contains(path)) {
            DiffEntry entry;
            entry.kind = DiffKind::Removed;
            entry.elementType = elementTypeName;
            entry.oldPath = path;
            report.entries.push_back(std::move(entry));
        }
    }
    for (const auto& [path, newPtr] : newIndex) {
        (void)newPtr;
        if (!oldIndex.contains(path)) {
            DiffEntry entry;
            entry.kind = DiffKind::Added;
            entry.elementType = elementTypeName;
            entry.newPath = path;
            report.entries.push_back(std::move(entry));
        }
    }
    for (const auto& [path, oldPtr] : oldIndex) {
        (void)oldPtr;
        if (newIndex.contains(path)) {
            DiffEntry entry;
            entry.kind = DiffKind::Modified;
            entry.elementType = elementTypeName;
            entry.oldPath = path;
            entry.newPath = path;
            // Empty fieldDiffs — PBI 3 (PAR-127) fills these in; this pass
            // only establishes that paths matched.
            report.entries.push_back(std::move(entry));
        }
    }
    return report;
}

// Whole-project matching: run matchByPath once per domain type across all
// files in each project, then merge the six reports. No move detection here —
// a path-only-in-old plus a different path-only-in-new stays a separate
// Removed/Added pair; PAR-117 revisits those.
[[nodiscard]] DiffReport matchParsedProjects(const ParsedProject& oldProject,
                                             const ParsedProject& newProject);
