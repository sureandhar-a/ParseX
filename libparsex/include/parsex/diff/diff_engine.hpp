#pragma once

#include <parsex/diff/diff_report.hpp>
#include <parsex/model/parsed_project.hpp>

// Public Diff Engine facade (PAR-135).
class DiffEngine {
public:
    DiffEngine() = default;

    // Runs, in order: PAR-116 path-based matching per domain type, merged
    // into one DiffReport → PAR-117 move detection over Removed/Added →
    // PAR-118 field-level diffing over every matched pair (including Moved
    // pairs) → PAR-119 sortDeterministically().
    //
    // Moved+Modified decision: an element that is both moved and
    // field-modified is a SINGLE DiffEntry with kind = Moved and a non-empty
    // fieldDiffs list, never two separate entries. Rationale: a reviewer
    // should see "this Frame moved from X to Y, and its DLC also changed" as
    // one coherent fact, not two disconnected ones.
    [[nodiscard]] DiffReport diff(const ParsedProject& oldProject,
                                  const ParsedProject& newProject) const;
};
