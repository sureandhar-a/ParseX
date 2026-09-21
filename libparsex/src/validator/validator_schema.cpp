#include <parsex/validator/validator.hpp>

#include <libxml/parser.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlschemas.h>

#include <parsex/raw/raw_document.hpp>
#include <parsex/schema/xml_schema_raii.hpp>

namespace {

std::string trimMessage(const char* raw) {
    std::string message = (raw != nullptr) ? raw : "schema validation failed";
    while (!message.empty() &&
           (message.back() == '\n' || message.back() == '\r' || message.back() == ' ')) {
        message.pop_back();
    }
    if (message.empty()) {
        message = "schema validation failed";
    }
    return message;
}

}  // namespace

void Validator::onSchemaError(void* userData, const xmlError* error) {
    auto* result = static_cast<ValidationResult*>(userData);
    if (result == nullptr || error == nullptr) {
        return;
    }
    ValidationError finding;
    finding.severity =
        (error->level == XML_ERR_WARNING) ? Severity::Warning : Severity::Error;
    finding.code = "schema.invalid";
    finding.message = trimMessage(error->message);
    // Location attribution is PAR-100's job — left unset here by design.
    result->errors.push_back(std::move(finding));
}

ValidationResult Validator::validateSchema(const ParsedFile& file,
                                           ::xmlSchema* sharedSchema) const {
    ValidationResult result;
    if (sharedSchema == nullptr) {
        result.errors.push_back({.severity = Severity::Error,
                                 .code = "schema.no_schema",
                                 .message = "validateSchema: null shared schema handle"});
        return result;
    }
    if (file.sourcePath.empty()) {
        result.errors.push_back({.severity = Severity::Error,
                                 .code = "schema.no_source",
                                 .message = "validateSchema: ParsedFile has no sourcePath"});
        return result;
    }

    // Fresh validation context per call (never shared across calls/threads).
    XmlSchemaValidCtxtPtr vctxt(xmlSchemaNewValidCtxt(sharedSchema));
    if (vctxt == nullptr) {
        result.errors.push_back({.severity = Severity::Error,
                                 .code = "schema.no_context",
                                 .message = "validateSchema: cannot create validation context"});
        return result;
    }
    // Structured (not printf-style) errors: callback gets xmlErrorPtr directly.
    xmlSchemaSetValidStructuredErrors(vctxt.get(), &Validator::onSchemaError, &result);

    const std::string narrowPath = file.sourcePath.string();
    XmlDocPtr doc(xmlReadFile(narrowPath.c_str(), nullptr, XML_PARSE_NONET));
    if (doc == nullptr) {
        result.errors.push_back({.severity = Severity::Error,
                                 .code = "schema.unreadable",
                                 .message = "validateSchema: cannot re-parse '" + narrowPath +
                                            "' for validation"});
        return result;
    }

    const int ret = xmlSchemaValidateDoc(vctxt.get(), doc.get());
    if (ret < 0) {
        // Internal error: no structured error was necessarily reported.
        result.errors.push_back({.severity = Severity::Error,
                                 .code = "schema.internal",
                                 .message = "validateSchema: internal validation error"});
    }
    // ret == 0 -> valid (no findings); ret > 0 -> findings already captured.
    return result;
}
