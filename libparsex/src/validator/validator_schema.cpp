#include <parsex/validator/validator.hpp>

#include <libxml/parser.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlschemas.h>

#include <parsex/raw/raw_document.hpp>
#include <parsex/schema/xml_schema_raii.hpp>

#include <algorithm>
#include <optional>
#include <ranges>

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

// Sorted (line -> span) index over one RawDocument. The Span-Tracking Loader
// already records RawSpan::lineNumber per node, so no second parse pass is
// needed — just flatten the tree. error->node is deliberately NOT trusted:
// it points into the throwaway re-parsed xmlDoc, not our RawNode tree, and
// occurrence-constraint violations can misattribute it to the wrong sibling.
using LineIndex = std::vector<std::pair<int, RawSpan>>;

void collectLineEntries(const RawNode& node, LineIndex& out) {
    if (node.span.lineNumber > 0) {
        out.emplace_back(static_cast<int>(node.span.lineNumber), node.span);
    }
    for (const auto& child : node.children) {
        collectLineEntries(*child, out);
    }
}

LineIndex buildLineIndex(const RawNode& root) {
    LineIndex index;
    collectLineEntries(root, index);
    std::ranges::sort(index, [](const auto& a, const auto& b) {
        if (a.first != b.first) {
            return a.first < b.first;
        }
        return a.second.startOffset < b.second.startOffset;
    });
    return index;
}

std::optional<RawSpan> spanForLine(const LineIndex& index, int line) {
    if (index.empty() || line <= 0) {
        return std::nullopt;
    }
    const auto it =
        std::ranges::lower_bound(index, line, {}, &std::pair<int, RawSpan>::first);
    if (it == index.end()) {
        return index.back().second;
    }
    if (it->first == line) {
        return it->second;
    }
    if (it == index.begin()) {
        return it->second;
    }
    const auto& higher = *it;
    const auto& lower = *(it - 1);
    const int loDist = line - lower.first;
    const int hiDist = higher.first - line;
    // Tie -> predecessor (the element whose start encloses the error line).
    return (hiDist < loDist) ? higher.second : lower.second;
}

struct SchemaErrorContext {
    ValidationResult* result = nullptr;
    const LineIndex* lineIndex = nullptr;
};

}  // namespace

void Validator::onSchemaError(void* userData, const xmlError* error) {
    auto* ctx = static_cast<SchemaErrorContext*>(userData);
    if (ctx == nullptr || ctx->result == nullptr || error == nullptr) {
        return;
    }
    ValidationError finding;
    finding.severity =
        (error->level == XML_ERR_WARNING) ? Severity::Warning : Severity::Error;
    finding.code = "schema.invalid";
    finding.message = trimMessage(error->message);
    // error->int2 (column) is best-effort only — not used for span lookup.
    if (ctx->lineIndex != nullptr) {
        finding.location = spanForLine(*ctx->lineIndex, error->line);
    }
    ctx->result->errors.push_back(std::move(finding));
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
    // Line-based lookup against our own tree (never error->node identity).
    LineIndex lineIndex;
    if (file.rawDocument != nullptr) {
        lineIndex = buildLineIndex(file.rawDocument->root);
    }
    SchemaErrorContext ctx{.result = &result, .lineIndex = &lineIndex};
    xmlSchemaSetValidStructuredErrors(vctxt.get(), &Validator::onSchemaError, &ctx);

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
