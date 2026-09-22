#pragma once

#include <iosfwd>
#include <string>
#include <vector>

// Plain data types the rest of the Diff Engine builds on (PAR-121).
//
// Matching (PAR-122), move detection (PAR-124), field diffing (PAR-127) and
// rendering (PAR-131..133) all consume/produce these types, so this header
// stays dependency-free by design: no model, parser, or validator includes.

enum class DiffKind { Added, Removed, Moved, Modified };

// Filled in by PBI 3 (PAR-127): field name plus old/new values as strings.
// String-ified via the diffFields toString() overload set (numeric types,
// strings, enums via per-enum name tables).
struct FieldDiff {
    std::string fieldName;
    std::string oldValue;
    std::string newValue;
};

struct DiffEntry {
    DiffKind kind = DiffKind::Added;
    std::string elementType;  // e.g. "Frame", "Signal"
    std::string oldPath;      // set for Removed, Moved, Modified
    std::string newPath;      // set for Added, Moved, Modified
    // Only meaningful for Modified / Moved-with-changes; empty otherwise.
    std::vector<FieldDiff> fieldDiffs;
};

// Ambiguity diagnostic (PAR-125): a secondary-key group that did not pair
// cleanly 1:1, so no Moved was inferred. The message names the shared key
// and lists candidate paths so a human can manually check.
struct DiffDiagnostic {
    std::string message;
    std::string elementType;
};

struct DiffReport {
    std::vector<DiffEntry> entries;
    std::vector<DiffDiagnostic> diagnostics;

    [[nodiscard]] bool empty() const noexcept { return entries.empty() && diagnostics.empty(); }
    [[nodiscard]] bool hasEntries() const noexcept { return !entries.empty(); }

    // Each domain-type matching pass builds its own small DiffReport; merge()
    // combines them into one. Appends other's entries, preserving relative
    // order within each side (PBI 4 sorts later).
    void merge(DiffReport&& other);
    void merge(const DiffReport& other);

    // Quick manual inspection during development (not the real renderer —
    // PBI 4 "Deterministic diff report ordering and rendering" owns the real
    // text/JSON output).
    [[nodiscard]] std::string toDebugString() const;
};

[[nodiscard]] std::string toString(DiffKind kind);
std::ostream& operator<<(std::ostream& os, DiffKind kind);
std::ostream& operator<<(std::ostream& os, const DiffEntry& entry);
std::ostream& operator<<(std::ostream& os, const DiffReport& report);
