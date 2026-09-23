#include <parsex/validator/validation_result.hpp>

#include <algorithm>

#include <parsex/json_contract/envelope.hpp>

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

namespace {

nlohmann::json errorToJson(const ValidationError& err) {
    nlohmann::json errJson;
    errJson["severity"] = err.severity == Severity::Warning ? "warning" : "error";
    errJson["code"] = err.code;
    errJson["message"] = err.message;
    // Omit-optional per CONVENTIONS.md: absent context travels as missing
    // keys, never explicit nulls.
    if (err.location.has_value()) {
        errJson["location"] = {
            {"startOffset", err.location->startOffset},
            {"endOffset", err.location->endOffset},
            {"lineNumber", err.location->lineNumber},
        };
    }
    if (err.path.has_value()) {
        errJson["path"] = *err.path;
    }
    if (err.expectedType.has_value()) {
        errJson["expectedType"] = *err.expectedType;
    }
    if (err.actualType.has_value()) {
        errJson["actualType"] = *err.actualType;
    }
    return errJson;
}

ValidationError errorFromJson(const nlohmann::json& errJson) {
    ValidationError err;
    err.severity =
        errJson.value("severity", "error") == "warning" ? Severity::Warning : Severity::Error;
    err.code = errJson.value("code", "");
    err.message = errJson.value("message", "");
    if (errJson.contains("location")) {
        const auto& loc = errJson.at("location");
        err.location = RawSpan{
            .startOffset = loc.value("startOffset", std::size_t{0}),
            .endOffset = loc.value("endOffset", std::size_t{0}),
            .lineNumber = loc.value("lineNumber", std::size_t{0}),
        };
    }
    if (errJson.contains("path")) {
        err.path = errJson.at("path").get<std::string>();
    }
    if (errJson.contains("expectedType")) {
        err.expectedType = errJson.at("expectedType").get<std::string>();
    }
    if (errJson.contains("actualType")) {
        err.actualType = errJson.at("actualType").get<std::string>();
    }
    return err;
}

}  // namespace

nlohmann::json ValidationResult::toJson() const {
    nlohmann::json payload;
    payload["passed"] = !hasErrors();
    payload["errors"] = nlohmann::json::array();
    for (const auto& err : errors) {
        payload["errors"].push_back(errorToJson(err));
    }
    return parsex::json_contract::wrapEnvelope("validationResult", std::move(payload));
}

ValidationResult ValidationResult::fromJson(const nlohmann::json& jsonDoc) {
    // Accept the full envelope or a bare payload object.
    const nlohmann::json& payload =
        (jsonDoc.is_object() && jsonDoc.contains("payload")) ? jsonDoc.at("payload") : jsonDoc;
    ValidationResult result;
    if (payload.contains("errors")) {
        for (const auto& errJson : payload.at("errors")) {
            result.errors.push_back(errorFromJson(errJson));
        }
    }
    return result;
}
