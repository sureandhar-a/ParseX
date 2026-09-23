#include <parsex/diff/diff_engine.hpp>

#include <parsex/diff/diff_match.hpp>
#include <parsex/diff/diff_moves.hpp>
#include <parsex/diff/diff_populate.hpp>

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): stateless-by-design instance API — callers write DiffEngine{}.diff(...), matching the Validator facade convention.
DiffReport DiffEngine::diff(const ParsedProject& oldProject,
                            const ParsedProject& newProject) const {
    DiffReport matched = matchParsedProjects(oldProject, newProject);
    DiffReport moved = applyMoveDetection(oldProject, newProject, std::move(matched));
    DiffReport populated = populateFieldDiffs(oldProject, newProject, std::move(moved));
    populated.sortDeterministically();
    return populated;
}
