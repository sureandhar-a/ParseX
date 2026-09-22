#include <parsex/diff/diff_report.hpp>

#include <iterator>
#include <ostream>
#include <sstream>

void DiffReport::merge(DiffReport&& other) {
    entries.insert(entries.end(), std::make_move_iterator(other.entries.begin()),
                   std::make_move_iterator(other.entries.end()));
    other.entries.clear();
}

void DiffReport::merge(const DiffReport& other) {
    entries.insert(entries.end(), other.entries.begin(), other.entries.end());
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
    os << "DiffReport(" << report.entries.size() << " entries)";
    for (const auto& entry : report.entries) {
        os << "\n  " << entry;
    }
    return os;
}

std::string DiffReport::toDebugString() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}
