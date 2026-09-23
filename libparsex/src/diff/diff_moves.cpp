#include <parsex/diff/diff_moves.hpp>

#include <algorithm>
#include <sstream>

#include <parsex/diff/diff_match.hpp>

namespace {

std::string joinSorted(std::vector<std::string> items, const char* sep = ",") {
    std::ranges::sort(items);
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            out += sep;
        }
        out += items[i];
    }
    return out;
}

}  // namespace

// TODO(move-keys): Frame has no numeric CAN ID in the current domain model
// (only length/transmitters/pdu mappings), and Signal carries no parent-Pdu
// identifier — the task's preferred secondary keys. The keys below use the
// most identity-like non-path fields available. If a CAN ID field is added to
// Frame later, prefer it here (and parent-Pdu + startBit for Signal).

std::optional<std::string> secondaryKey(const Cluster& cluster) {
    std::ostringstream oss;
    oss << "baud:";
    if (cluster.baudrate.has_value()) {
        oss << *cluster.baudrate;
    } else {
        oss << "-";
    }
    oss << "|ch:" << joinSorted(cluster.physicalChannels);
    return oss.str();
}

std::optional<std::string> secondaryKey(const EcuInstance& ecu) {
    std::ostringstream oss;
    oss << "ch:" << joinSorted(ecu.connectedChannels);
    oss << "|ctrl:" << joinSorted(ecu.controllers);
    return oss.str();
}

std::optional<std::string> secondaryKey(const Frame& frame) {
    std::ostringstream oss;
    oss << "len:" << frame.length;
    oss << "|tx:" << joinSorted(frame.transmitters);
    std::vector<std::string> mappings;
    mappings.reserve(frame.pdus.size());
    for (const auto& mapping : frame.pdus) {
        mappings.push_back(mapping.pduShortNameRef + "@" + std::to_string(mapping.startPosition));
    }
    oss << "|pdu:" << joinSorted(mappings);
    return oss.str();
}

std::optional<std::string> secondaryKey(const Pdu& pdu) {
    std::ostringstream oss;
    oss << "len:" << pdu.length;
    std::vector<std::string> mappings;
    mappings.reserve(pdu.signalMappings.size());
    for (const auto& mapping : pdu.signalMappings) {
        mappings.push_back(mapping.signalShortNameRef + "@" + std::to_string(mapping.startPosition));
    }
    oss << "|sig:" << joinSorted(mappings);
    return oss.str();
}

std::optional<std::string> secondaryKey(const Signal& signal) {
    std::ostringstream oss;
    oss << "start:" << signal.startBit << "|len:" << signal.bitLength;
    oss << "|order:"
        << (signal.byteOrder == ByteOrder::MostSignificantByteFirst ? "M" : "L");
    oss << "|rx:" << joinSorted(signal.receivers);
    return oss.str();
}

std::optional<std::string> secondaryKey(const SignalGroup& group) {
    return std::string("m:") + joinSorted(group.members);
}

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

DiffReport applyMoveDetection(const ParsedProject& oldProject, const ParsedProject& newProject,
                              DiffReport matched) {
    const auto oldClusters = collectAll<Cluster>(oldProject, &ParsedFile::clusters);
    const auto newClusters = collectAll<Cluster>(newProject, &ParsedFile::clusters);
    detectMovesForType(indexByPath(oldClusters), indexByPath(newClusters), matched, "Cluster");

    const auto oldEcus = collectAll<EcuInstance>(oldProject, &ParsedFile::ecuInstances);
    const auto newEcus = collectAll<EcuInstance>(newProject, &ParsedFile::ecuInstances);
    detectMovesForType(indexByPath(oldEcus), indexByPath(newEcus), matched, "EcuInstance");

    const auto oldFrames = collectAll<Frame>(oldProject, &ParsedFile::frames);
    const auto newFrames = collectAll<Frame>(newProject, &ParsedFile::frames);
    detectMovesForType(indexByPath(oldFrames), indexByPath(newFrames), matched, "Frame");

    const auto oldPdus = collectAll<Pdu>(oldProject, &ParsedFile::pdus);
    const auto newPdus = collectAll<Pdu>(newProject, &ParsedFile::pdus);
    detectMovesForType(indexByPath(oldPdus), indexByPath(newPdus), matched, "Pdu");

    const auto oldSignals = collectAll<Signal>(oldProject, &ParsedFile::signals);
    const auto newSignals = collectAll<Signal>(newProject, &ParsedFile::signals);
    detectMovesForType(indexByPath(oldSignals), indexByPath(newSignals), matched, "Signal");

    const auto oldGroups = collectAll<SignalGroup>(oldProject, &ParsedFile::signalGroups);
    const auto newGroups = collectAll<SignalGroup>(newProject, &ParsedFile::signalGroups);
    detectMovesForType(indexByPath(oldGroups), indexByPath(newGroups), matched, "SignalGroup");

    return matched;
}
