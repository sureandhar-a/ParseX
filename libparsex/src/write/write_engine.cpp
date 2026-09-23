#include <parsex/write/write_engine.hpp>

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
    for (const Cluster& c :
         sortedCopy(std::move(clusters), [](const Cluster& v) { return v.common.shortName; })) {
        xmlAddChild(elements, buildClusterElement(ctx.doc(), c));
    }
    for (const EcuInstance& e : sortedCopy(std::move(ecus), [](const EcuInstance& v) {
             return v.common.shortName;
         })) {
        xmlAddChild(elements, buildEcuInstanceElement(ctx.doc(), e));
    }
    for (const Frame& f :
         sortedCopy(std::move(frames), [](const Frame& v) { return v.common.shortName; })) {
        xmlAddChild(elements, buildFrameElement(ctx.doc(), f));
    }
    for (const Pdu& p :
         sortedCopy(std::move(pdus), [](const Pdu& v) { return v.common.shortName; })) {
        xmlAddChild(elements, buildPduElement(ctx.doc(), p));
    }
    for (const Signal& s :
         sortedCopy(std::move(signals), [](const Signal& v) { return v.common.shortName; })) {
        xmlAddChild(elements, buildSignalElement(ctx.doc(), s));
    }
    for (const SignalGroup& g : sortedCopy(std::move(groups), [](const SignalGroup& v) {
             return v.common.shortName;
         })) {
        xmlAddChild(elements, buildSignalGroupElement(ctx.doc(), g));
    }

    assertNoWhitespaceOnlyTextNodes(ctx.root());
    writeXmlToFile(ctx.doc(), outputPath.string());
}
