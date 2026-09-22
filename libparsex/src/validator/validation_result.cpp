#include <parsex/validator/validation_result.hpp>

#include <algorithm>

bool ValidationResult::hasErrors() const noexcept {
    return std::ranges::any_of(
        errors, [](const ValidationError& err) { return err.severity == Severity::Error; });
}

bool ValidationResult::hasWarnings() const noexcept {
    return std::ranges::any_of(errors, [](const ValidationError& err) {
        return err.severity == Severity::Warning;
    });
}

void ValidationResult::merge(const ValidationResult& other) {
    errors.insert(errors.end(), other.errors.begin(), other.errors.end());
}
