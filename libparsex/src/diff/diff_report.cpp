#include <parsex/diff/diff_report.hpp>

#include <algorithm>
#include <iterator>
#include <ostream>
#include <sstream>

// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved): members are moved out via named locals below (then cleared); the parameter itself is intentionally not relocated wholesale.
void DiffReport::merge(DiffReport&& other) {
    std::vector<DiffEntry> incoming = std::move(other.entries);
    entries.insert(entries.end(), std::make_move_iterator(incoming.begin()),
                   std::make_move_iterator(incoming.end()));
    other.entries.clear();
    std::vector<DiffDiagnostic> incomingDiags = std::move(other.diagnostics);
    diagnostics.insert(diagnostics.end(), std::make_move_iterator(incomingDiags.begin()),
                       std::make_move_iterator(incomingDiags.end()));
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

std::ostream& operator<<(std::ostream& stream, DiffKind kind) {
    return stream << toString(kind);
}

std::ostream& operator<<(std::ostream& stream, const DiffEntry& entry) {
    stream << "[" << entry.kind << "] " << entry.elementType;
    switch (entry.kind) {
        case DiffKind::Added:
            stream << " new=\"" << entry.newPath << "\"";
            break;
        case DiffKind::Removed:
            stream << " old=\"" << entry.oldPath << "\"";
            break;
        case DiffKind::Moved:
            stream << " old=\"" << entry.oldPath << "\" new=\"" << entry.newPath << "\"";
            break;
        case DiffKind::Modified:
            stream << " path=\"" << entry.newPath << "\"";
            break;
    }
    stream << " fields=" << entry.fieldDiffs.size();
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const DiffReport& report) {
    stream << "DiffReport(" << report.entries.size() << " entries, " << report.diagnostics.size()
           << " diagnostics)";
    for (const auto& entry : report.entries) {
        stream << "\n  " << entry;
    }
    for (const auto& diag : report.diagnostics) {
        stream << "\n  [Diagnostic] " << diag.elementType << ": " << diag.message;
    }
    return stream;
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
    std::ranges::sort(entries,
                      [](const DiffEntry& first, const DiffEntry& second) {
                          const std::string firstKey = sortKeyFor(first);
                          const std::string secondKey = sortKeyFor(second);
                          if (firstKey != secondKey) {
                              return firstKey < secondKey;
                          }
                          return first.elementType < second.elementType;
                      });
    std::ranges::sort(diagnostics,
                      [](const DiffDiagnostic& first, const DiffDiagnostic& second) {
                          if (first.message != second.message) {
                              return first.message < second.message;
                          }
                          return first.elementType < second.elementType;
                      });
}
