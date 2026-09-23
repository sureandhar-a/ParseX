#include <parsex/write/write_engine.hpp>

#include <parsex/telemetry/scoped_span.hpp>
#include <parsex/telemetry/scoped_span_macro.hpp>
#include <parsex/write/write_context.hpp>
#include <parsex/write/write_elements.hpp>
#include <parsex/write/write_format.hpp>
#include <parsex/write/write_ordering.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace {

template <typename T, typename KeyFn>
std::vector<T> sortedCopy(std::vector<T> items, KeyFn key) {
    std::ranges::stable_sort(items, {}, key);
    return items;
}

}  // namespace

void WriteEngine::write(const ParsedProject& project,
                        const std::filesystem::path& outputPath) const {
    parsex::telemetry::ScopedSpan span("writeEngine.write");
    span.setAttribute("outputPath", outputPath.string());
    span.setAttribute("fileCount", static_cast<std::int64_t>(project.files.size()));
    const ValidationResult problems = [&] {
        PARSEX_SPAN("writeEngine.validate");
        return validate(project);
    }();
    if (problems.hasErrors()) {
        throw WriteError(problems);
    }
    // Schema version: first file's release wins (single-release projects in
    // v1); empty projects default to 4.4.0 (the fixture release).
    std::string release = "4.4.0";
    if (!project.files.empty() && !project.files.front().autosarRelease.empty()) {
        release = project.files.front().autosarRelease;
    }
    WriteContext ctx = WriteContext::forRelease(release);

    xmlNodePtr package = xmlNewChild(ctx.arPackages(), nullptr, BAD_CAST "AR-PACKAGE", nullptr);
    if (package == nullptr) {
        throw std::runtime_error("parsex: cannot create AR-PACKAGE element");
    }
    appendTextChild(package, "SHORT-NAME", "Sys");
    xmlNodePtr elements = xmlNewChild(package, nullptr, BAD_CAST "ELEMENTS", nullptr);
    if (elements == nullptr) {
        throw std::runtime_error("parsex: cannot create ELEMENTS element");
    }

    // Merge all files' domain objects (v1: single package "Sys"), emitting in
    // fixed type-group order with each group sorted by short-name
    // (semantically unordered per TPS_ASR_00014).
    {
        PARSEX_SPAN("writeEngine.buildTree");
    std::vector<Cluster> clusters;
    std::vector<EcuInstance> ecus;
    std::vector<Frame> frames;
    std::vector<Pdu> pdus;
    std::vector<Signal> signals;
    std::vector<SignalGroup> groups;
    for (const ParsedFile& file : project.files) {
        clusters.insert(clusters.end(), file.clusters.begin(), file.clusters.end());
        ecus.insert(ecus.end(), file.ecuInstances.begin(), file.ecuInstances.end());
        frames.insert(frames.end(), file.frames.begin(), file.frames.end());
        pdus.insert(pdus.end(), file.pdus.begin(), file.pdus.end());
        signals.insert(signals.end(), file.signals.begin(), file.signals.end());
        groups.insert(groups.end(), file.signalGroups.begin(), file.signalGroups.end());
    }
    for (const Cluster& cluster : sortedCopy(std::move(clusters), [](const Cluster& value) {
             return value.common.shortName;
         })) {
        xmlAddChild(elements, buildClusterElement(ctx.doc(), cluster));
    }
    for (const EcuInstance& ecu : sortedCopy(std::move(ecus), [](const EcuInstance& value) {
             return value.common.shortName;
         })) {
        xmlAddChild(elements, buildEcuInstanceElement(ctx.doc(), ecu));
    }
    for (const Frame& frame :
         sortedCopy(std::move(frames), [](const Frame& value) { return value.common.shortName; })) {
        xmlAddChild(elements, buildFrameElement(ctx.doc(), frame));
    }
    for (const Pdu& pdu :
         sortedCopy(std::move(pdus), [](const Pdu& value) { return value.common.shortName; })) {
        xmlAddChild(elements, buildPduElement(ctx.doc(), pdu));
    }
    for (const Signal& signal : sortedCopy(std::move(signals), [](const Signal& value) {
             return value.common.shortName;
         })) {
        xmlAddChild(elements, buildSignalElement(ctx.doc(), signal));
    }
    for (const SignalGroup& group : sortedCopy(std::move(groups), [](const SignalGroup& value) {
             return value.common.shortName;
         })) {
        xmlAddChild(elements, buildSignalGroupElement(ctx.doc(), group));
    }
    }  // writeEngine.buildTree

    assertNoWhitespaceOnlyTextNodes(ctx.root());
    // Temp path + rename into place (same convention as writeCacheAtomically):
    // a failed write() never leaves a partially-written file at outputPath.
    {
        PARSEX_SPAN("writeEngine.writeFile");
    const std::filesystem::path tempPath =
        std::filesystem::path(outputPath.string() + ".parsex-tmp");
    try {
        writeXmlToFile(ctx.doc(), tempPath.string());
        std::error_code renameError;
        std::filesystem::rename(tempPath, outputPath, renameError);
        if (renameError) {
            throw std::runtime_error("parsex: failed to move written file into place '" +
                                     outputPath.string() + "': " + renameError.message());
        }
    } catch (...) {
        std::error_code dropError;
        std::filesystem::remove(tempPath, dropError);
        throw;
    }
    }  // writeEngine.writeFile
}

WriteError::WriteError(ValidationResult errors)
    : std::runtime_error([&] {
          std::string message = "parsex: cannot write project:";
          for (const ValidationError& error : errors.errors) {
              message += " [" + error.code + "] " + error.message;
          }
          return message;
      }()),
      result(std::move(errors)) {}

namespace {

void checkShortNames(const std::string& type,
                     const std::vector<std::string>& shortNames, std::size_t fileIndex,
                     ValidationResult& out) {
    out.errors.reserve(out.errors.size() + shortNames.size());
    for (std::size_t idx = 0; idx < shortNames.size(); ++idx) {
        if (!shortNames.at(idx).empty()) {
            continue;
        }
        ValidationError error;
        error.severity = Severity::Error;
        error.code = "write.missing_field";
        error.message = type + "[" + std::to_string(idx) + "] in file[" +
                        std::to_string(fileIndex) + "] is missing required SHORT-NAME";
        error.path = type + "[" + std::to_string(idx) + "]";
        out.errors.push_back(std::move(error));
    }
}

}  // namespace

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): stateless-by-design instance API — callers write WriteEngine{}.validate(...).
ValidationResult WriteEngine::validate(const ParsedProject& project) const {
    ValidationResult result;
    for (std::size_t fileIdx = 0; fileIdx < project.files.size(); ++fileIdx) {
        const ParsedFile& file = project.files.at(fileIdx);
        const auto names = [](const auto& items) {
            std::vector<std::string> out;
            out.reserve(items.size());
            for (const auto& item : items) {
                out.push_back(item.common.shortName);
            }
            return out;
        };
        checkShortNames("Cluster", names(file.clusters), fileIdx, result);
        checkShortNames("EcuInstance", names(file.ecuInstances), fileIdx, result);
        checkShortNames("Frame", names(file.frames), fileIdx, result);
        checkShortNames("Pdu", names(file.pdus), fileIdx, result);
        checkShortNames("Signal", names(file.signals), fileIdx, result);
        checkShortNames("SignalGroup", names(file.signalGroups), fileIdx, result);
    }
    return result;
}
