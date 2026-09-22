#include <parsex/diff/diff_report.hpp>

#include <sstream>

// Human-readable text rendering (PAR-132).
//
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
