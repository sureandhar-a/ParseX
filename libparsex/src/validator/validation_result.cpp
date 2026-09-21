#include <parsex/validator/validation_result.hpp>

bool ValidationResult::hasErrors() const noexcept {
    for (const auto& err : errors) {
        if (err.severity == Severity::Error) {
            return true;
        }
    }
    return false;
}

bool ValidationResult::hasWarnings() const noexcept {
    for (const auto& err : errors) {
        if (err.severity == Severity::Warning) {
            return true;
        }
    }
    return false;
}

void ValidationResult::merge(const ValidationResult& other) {
    errors.insert(errors.end(), other.errors.begin(), other.errors.end());
}
