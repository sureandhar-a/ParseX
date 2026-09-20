#include <parsex/parser/parser.hpp>

#include <parsex/parser/loader.hpp>
#include <parsex/parser/model_builder.hpp>
#include <parsex/parser/release_detector.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Dispatches one raw node into its domain vector (recursed over the whole
// tree below). Tags outside the six families are skipped — other AUTOSAR
// content (companies, mappings, COMPU-METHODs, ...) is not ParseX domain data.
void buildSubtree(const RawNode& node, ParsedFile& file) {
    const std::string& tag = node.tagName;
    if (tag == "CLUSTER" || tag == "CAN-CLUSTER") {
        file.clusters.push_back(buildCluster(node, file.warnings));
    } else if (tag == "ECU-INSTANCE") {
        file.ecuInstances.push_back(buildEcuInstance(node, file.warnings));
    } else if (tag == "FRAME" || tag == "CAN-FRAME") {
        file.frames.push_back(buildFrame(node, file.warnings));
    } else if (tag == "PDU" || tag == "I-SIGNAL-I-PDU") {
        file.pdus.push_back(buildPdu(node, file.warnings));
    } else if (tag == "SYSTEM-SIGNAL" || tag == "I-SIGNAL") {
        file.signals.push_back(buildSignal(node, file.warnings));
    } else if (tag == "SIGNAL-GROUP" || tag == "I-SIGNAL-GROUP") {
        file.signalGroups.push_back(buildSignalGroup(node, file.warnings));
    }
    for (const auto& child : node.children) {
        buildSubtree(*child, file);
    }
}

}  // namespace

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): stateless-by-design instance API — callers write Parser{}.parseFile(...).
ParsedFile Parser::parseFile(const std::filesystem::path& path) const {
    RawDocument document = loadRawDocument(path);

    const std::optional<std::string> schemaFilename = detectSchemaFilename(document);
    if (!schemaFilename.has_value()) {
        throw std::runtime_error("parsex: cannot determine AUTOSAR release for '" +
                                 path.string() +
                                 "': root element has no xsi:schemaLocation");
    }

    ParsedFile file;
    file.autosarRelease = resolveRelease(schemaFilename.value());
    file.sourcePath = path;
    buildSubtree(document.root, file);
    // Document move re-points parent links at the shared copy (see
    // RawDocument's move operations) — the Write Engine's spans stay valid.
    file.rawDocument = std::make_shared<RawDocument>(std::move(document));
    return file;
}
