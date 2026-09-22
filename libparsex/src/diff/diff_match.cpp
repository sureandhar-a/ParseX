#include <parsex/diff/diff_match.hpp>

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

}  // namespace

DiffReport matchParsedProjects(const ParsedProject& oldProject, const ParsedProject& newProject) {
    DiffReport report;

    const auto oldClusters = collectAll<Cluster>(oldProject, &ParsedFile::clusters);
    const auto newClusters = collectAll<Cluster>(newProject, &ParsedFile::clusters);
    report.merge(matchByPath(indexByPath(oldClusters), indexByPath(newClusters), "Cluster"));

    const auto oldEcus = collectAll<EcuInstance>(oldProject, &ParsedFile::ecuInstances);
    const auto newEcus = collectAll<EcuInstance>(newProject, &ParsedFile::ecuInstances);
    report.merge(matchByPath(indexByPath(oldEcus), indexByPath(newEcus), "EcuInstance"));

    const auto oldFrames = collectAll<Frame>(oldProject, &ParsedFile::frames);
    const auto newFrames = collectAll<Frame>(newProject, &ParsedFile::frames);
    report.merge(matchByPath(indexByPath(oldFrames), indexByPath(newFrames), "Frame"));

    const auto oldPdus = collectAll<Pdu>(oldProject, &ParsedFile::pdus);
    const auto newPdus = collectAll<Pdu>(newProject, &ParsedFile::pdus);
    report.merge(matchByPath(indexByPath(oldPdus), indexByPath(newPdus), "Pdu"));

    const auto oldSignals = collectAll<Signal>(oldProject, &ParsedFile::signals);
    const auto newSignals = collectAll<Signal>(newProject, &ParsedFile::signals);
    report.merge(matchByPath(indexByPath(oldSignals), indexByPath(newSignals), "Signal"));

    const auto oldGroups = collectAll<SignalGroup>(oldProject, &ParsedFile::signalGroups);
    const auto newGroups = collectAll<SignalGroup>(newProject, &ParsedFile::signalGroups);
    report.merge(matchByPath(indexByPath(oldGroups), indexByPath(newGroups), "SignalGroup"));

    return report;
}
