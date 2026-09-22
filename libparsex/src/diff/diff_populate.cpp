#include <parsex/diff/diff_populate.hpp>

#include <parsex/diff/diff_fields.hpp>
#include <parsex/diff/diff_match.hpp>
#include <parsex/diff/diff_nested.hpp>
#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>

namespace {

template <typename T, typename Member>
std::vector<T> collectAll(const ParsedProject& project, Member member) {
    std::vector<T> all;
    for (const auto& file : project.files) {
        const std::vector<T>& vec = file.*member;
        all.insert(all.end(), vec.begin(), vec.end());
    }
    return all;
}

template <typename T>
const T* lookup(const std::map<std::string, const T*>& index, const std::string& path) {
    const auto it = index.find(path);
    return it != index.end() ? it->second : nullptr;
}

std::vector<FieldDiff> diffPair(const std::string& type, const std::string& oldPath,
                                const std::string& newPath,
                                const std::map<std::string, const Cluster*>& oldClusters,
                                const std::map<std::string, const Cluster*>& newClusters,
                                const std::map<std::string, const EcuInstance*>& oldEcus,
                                const std::map<std::string, const EcuInstance*>& newEcus,
                                const std::map<std::string, const Frame*>& oldFrames,
                                const std::map<std::string, const Frame*>& newFrames,
                                const std::map<std::string, const Pdu*>& oldPdus,
                                const std::map<std::string, const Pdu*>& newPdus,
                                const std::map<std::string, const Signal*>& oldSignals,
                                const std::map<std::string, const Signal*>& newSignals,
                                const std::map<std::string, const SignalGroup*>& oldGroups,
                                const std::map<std::string, const SignalGroup*>& newGroups) {
    if (type == "Cluster") {
        const Cluster* o = lookup(oldClusters, oldPath);
        const Cluster* n = lookup(newClusters, newPath);
        return (o != nullptr && n != nullptr) ? diffStruct(*o, *n) : std::vector<FieldDiff>{};
    }
    if (type == "EcuInstance") {
        const EcuInstance* o = lookup(oldEcus, oldPath);
        const EcuInstance* n = lookup(newEcus, newPath);
        return (o != nullptr && n != nullptr) ? diffStruct(*o, *n) : std::vector<FieldDiff>{};
    }
    if (type == "Frame") {
        const Frame* o = lookup(oldFrames, oldPath);
        const Frame* n = lookup(newFrames, newPath);
        return (o != nullptr && n != nullptr) ? diffFrameElements(*o, *n) : std::vector<FieldDiff>{};
    }
    if (type == "Pdu") {
        const Pdu* o = lookup(oldPdus, oldPath);
        const Pdu* n = lookup(newPdus, newPath);
        return (o != nullptr && n != nullptr) ? diffPduElements(*o, *n) : std::vector<FieldDiff>{};
    }
    if (type == "Signal") {
        const Signal* o = lookup(oldSignals, oldPath);
        const Signal* n = lookup(newSignals, newPath);
        return (o != nullptr && n != nullptr) ? diffSignalElements(*o, *n)
                                              : std::vector<FieldDiff>{};
    }
    if (type == "SignalGroup") {
        const SignalGroup* o = lookup(oldGroups, oldPath);
        const SignalGroup* n = lookup(newGroups, newPath);
        return (o != nullptr && n != nullptr) ? diffSignalGroupElements(*o, *n)
                                              : std::vector<FieldDiff>{};
    }
    return {};
}

}  // namespace

DiffReport populateFieldDiffs(const ParsedProject& oldProject, const ParsedProject& newProject,
                              DiffReport report) {
    const auto oldClusters = collectAll<Cluster>(oldProject, &ParsedFile::clusters);
    const auto newClusters = collectAll<Cluster>(newProject, &ParsedFile::clusters);
    const auto oldEcus = collectAll<EcuInstance>(oldProject, &ParsedFile::ecuInstances);
    const auto newEcus = collectAll<EcuInstance>(newProject, &ParsedFile::ecuInstances);
    const auto oldFrames = collectAll<Frame>(oldProject, &ParsedFile::frames);
    const auto newFrames = collectAll<Frame>(newProject, &ParsedFile::frames);
    const auto oldPdus = collectAll<Pdu>(oldProject, &ParsedFile::pdus);
    const auto newPdus = collectAll<Pdu>(newProject, &ParsedFile::pdus);
    const auto oldSignals = collectAll<Signal>(oldProject, &ParsedFile::signals);
    const auto newSignals = collectAll<Signal>(newProject, &ParsedFile::signals);
    const auto oldGroups = collectAll<SignalGroup>(oldProject, &ParsedFile::signalGroups);
    const auto newGroups = collectAll<SignalGroup>(newProject, &ParsedFile::signalGroups);

    const auto oldClusterIdx = indexByPath(oldClusters);
    const auto newClusterIdx = indexByPath(newClusters);
    const auto oldEcuIdx = indexByPath(oldEcus);
    const auto newEcuIdx = indexByPath(newEcus);
    const auto oldFrameIdx = indexByPath(oldFrames);
    const auto newFrameIdx = indexByPath(newFrames);
    const auto oldPduIdx = indexByPath(oldPdus);
    const auto newPduIdx = indexByPath(newPdus);
    const auto oldSignalIdx = indexByPath(oldSignals);
    const auto newSignalIdx = indexByPath(newSignals);
    const auto oldGroupIdx = indexByPath(oldGroups);
    const auto newGroupIdx = indexByPath(newGroups);

    std::vector<DiffEntry> kept;
    kept.reserve(report.entries.size());
    for (auto& entry : report.entries) {
        if (entry.kind != DiffKind::Modified && entry.kind != DiffKind::Moved) {
            kept.push_back(std::move(entry));
            continue;
        }
        entry.fieldDiffs =
            diffPair(entry.elementType, entry.oldPath, entry.newPath, oldClusterIdx, newClusterIdx, oldEcuIdx, newEcuIdx, oldFrameIdx, newFrameIdx,
                     oldPduIdx, newPduIdx, oldSignalIdx, newSignalIdx, oldGroupIdx, newGroupIdx);
        if (!entry.fieldDiffs.empty()) {
            kept.push_back(std::move(entry));
        }
    }
    report.entries = std::move(kept);
    return report;
}
