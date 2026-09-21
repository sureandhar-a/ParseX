#include <parsex/validator/validator.hpp>

// validateSchema() runs once per file (it needs each file's sourcePath +
// rawDocument for re-parse and span lookup); the other three run once for
// the whole project. Merge preserves every finding's original severity in
// both modes — strictness lives only in overallPassed().
ValidationResult Validator::validateAll(const ParsedProject& project,
                                        ::xmlSchema* sharedSchema) const {
    ValidationResult combined;
    for (const auto& file : project.files) {
        combined.merge(validateSchema(file, sharedSchema));
    }
    combined.merge(validateReferences(project));
    combined.merge(validateUniqueness(project));
    combined.merge(validateCanSemantics(project));
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
