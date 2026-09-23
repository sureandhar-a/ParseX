#include <parsex/diff/diff_engine.hpp>

#include <parsex/diff/diff_match.hpp>
#include <parsex/diff/diff_moves.hpp>
#include <parsex/diff/diff_populate.hpp>
#include <parsex/telemetry/scoped_span.hpp>
#include <parsex/telemetry/scoped_span_macro.hpp>

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): stateless-by-design instance API — callers write DiffEngine{}.diff(...), matching the Validator facade convention.
DiffReport DiffEngine::diff(const ParsedProject& oldProject,
                            const ParsedProject& newProject) const {
    parsex::telemetry::ScopedSpan span("diffEngine.diff");
    DiffReport matched = [&] {
        PARSEX_SPAN("diffEngine.match");
        return matchParsedProjects(oldProject, newProject);
    }();
    DiffReport moved = [&] {
        PARSEX_SPAN("diffEngine.moveDetection");
        return applyMoveDetection(oldProject, newProject, std::move(matched));
    }();
    DiffReport populated = [&] {
        PARSEX_SPAN("diffEngine.fieldDiff");
        return populateFieldDiffs(oldProject, newProject, std::move(moved));
    }();
    {
        PARSEX_SPAN("diffEngine.render");
        populated.sortDeterministically();
    }
    span.setAttribute("entryCount", static_cast<std::int64_t>(populated.entries.size()));
    return populated;
}
