#pragma once

#include <parsex/diff/diff_report.hpp>
#include <parsex/model/parsed_project.hpp>

// Field-diff wiring (PAR-129).
//
// Replaces PAR-116's placeholder Modified entries (matched paths, empty
// fieldDiffs) with real field-level data from diffStruct()/nested helpers.
// A Modified entry with zero real differences is dropped entirely — the
// placeholder was only internal bookkeeping. Moved entries are also field
// diffed but always kept (a plain move with empty fieldDiffs is still a real
// finding; the facade in PAR-135 relies on this). Added/Removed entries pass
// through untouched.
[[nodiscard]] DiffReport populateFieldDiffs(const ParsedProject& oldProject,
                                            const ParsedProject& newProject, DiffReport report);
