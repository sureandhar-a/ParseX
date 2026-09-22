#include <parsex/diff/diff_report.hpp>

#include <algorithm>
#include <iterator>
#include <ostream>
#include <sstream>

void DiffReport::merge(DiffReport&& other) {
    entries.insert(entries.end(), std::make_move_iterator(other.entries.begin()),
                   std::make_move_iterator(other.entries.end()));
    other.entries.clear();
    diagnostics.insert(diagnostics.end(), std::make_move_iterator(other.diagnostics.begin()),
                       std::make_move_iterator(other.diagnostics.end()));
    other.diagnostics.clear();
}

void DiffReport::merge(const DiffReport& other) {
    entries.insert(entries.end(), other.entries.begin(), other.entries.end());
    diagnostics.insert(diagnostics.end(), other.diagnostics.begin(), other.diagnostics.end());
}

std::string toString(DiffKind kind) {
    switch (kind) {
        case DiffKind::Added:
            return "Added";
        case DiffKind::Removed:
            return "Removed";
        case DiffKind::Moved:
            return "Moved";
        case DiffKind::Modified:
            return "Modified";
    }
    return "Unknown";
}

std::ostream& operator<<(std::ostream& os, DiffKind kind) { return os << toString(kind); }

std::ostream& operator<<(std::ostream& os, const DiffEntry& entry) {
    os << "[" << entry.kind << "] " << entry.elementType;
    switch (entry.kind) {
        case DiffKind::Added:
            os << " new=\"" << entry.newPath << "\"";
            break;
        case DiffKind::Removed:
            os << " old=\"" << entry.oldPath << "\"";
            break;
        case DiffKind::Moved:
            os << " old=\"" << entry.oldPath << "\" new=\"" << entry.newPath << "\"";
            break;
        case DiffKind::Modified:
            os << " path=\"" << entry.newPath << "\"";
            break;
    }
    os << " fields=" << entry.fieldDiffs.size();
    return os;
}

std::ostream& operator<<(std::ostream& os, const DiffReport& report) {
    os << "DiffReport(" << report.entries.size() << " entries, " << report.diagnostics.size()
       << " diagnostics)";
    for (const auto& entry : report.entries) {
        os << "\n  " << entry;
    }
    for (const auto& diag : report.diagnostics) {
        os << "\n  [Diagnostic] " << diag.elementType << ": " << diag.message;
    }
    return os;
}

std::string DiffReport::toDebugString() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}

namespace {

// Stable sort key per entry: newPath for Added/Modified, oldPath for
// Removed, and — consistently — newPath for Moved (a moved element's new
// location is what a reviewer looks up; documented here so future readers
// don't have to guess which path Moved sorts by).
std::string sortKeyFor(const DiffEntry& entry) {
    switch (entry.kind) {
        case DiffKind::Added:
        case DiffKind::Modified:
        case DiffKind::Moved:
            return entry.newPath;
        case DiffKind::Removed:
            return entry.oldPath;
    }
    return {};
}

}  // namespace

void DiffReport::sortDeterministically() {
    std::sort(entries.begin(), entries.end(), [](const DiffEntry& a, const DiffEntry& b) {
        const std::string ka = sortKeyFor(a);
        const std::string kb = sortKeyFor(b);
        if (ka != kb) {
            return ka < kb;
        }
        return a.elementType < b.elementType;
    });
    std::sort(diagnostics.begin(), diagnostics.end(), [](const DiffDiagnostic& a,
                                                          const DiffDiagnostic& b) {
        if (a.message != b.message) {
            return a.message < b.message;
        }
        return a.elementType < b.elementType;
    });
}
