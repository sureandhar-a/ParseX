#include <parsex/validator/validator.hpp>

#include <parsex/telemetry/scoped_span.hpp>
#include <parsex/telemetry/scoped_span_macro.hpp>

// validateSchema() runs once per file (it needs each file's sourcePath +
// rawDocument for re-parse and span lookup); the other three run once for
// the whole project. Merge preserves every finding's original severity in
// both modes — strictness lives only in overallPassed().
ValidationResult Validator::validateAll(const ParsedProject& project,
                                        ::xmlSchema* sharedSchema) const {
    parsex::telemetry::ScopedSpan span("validator.validateAll");
    span.setAttribute("fileCount", static_cast<std::int64_t>(project.files.size()));
    ValidationResult combined;
    {
        PARSEX_SPAN("validator.schema");
        for (const auto& file : project.files) {
            combined.merge(validateSchema(file, sharedSchema));
        }
    }
    {
        PARSEX_SPAN("validator.references");
        combined.merge(validateReferences(project));
    }
    {
        PARSEX_SPAN("validator.uniqueness");
        combined.merge(validateUniqueness(project));
    }
    {
        PARSEX_SPAN("validator.can");
        combined.merge(validateCanSemantics(project));
    }
    return combined;
}

bool Validator::overallPassed(const ValidationResult& result, StrictMode mode) {
    if (result.hasErrors()) {
        return false;
    }
    if (mode == StrictMode::Strict && result.hasWarnings()) {
        return false;
    }
    return true;
}
