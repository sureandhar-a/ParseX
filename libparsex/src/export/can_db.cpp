#include <parsex/export/can_db.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

std::string trimText(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// Joins are by short name (text after the last '/'), same accepted v1
// limitation as the model builder.
std::string shortNameOfRef(const std::string& ref) {
    const std::string text = trimText(ref);
    const std::size_t slash = text.rfind('/');
    return slash == std::string::npos ? text : text.substr(slash + 1);
}

const RawNode* findChild(const RawNode& node, const std::string& tag) {
    for (const auto& child : node.children) {
        if (child->tagName == tag) {
            return child.get();
        }
    }
    return nullptr;
}

const RawNode* findDescendant(const RawNode& node, const std::string& tag) {
    for (const auto& child : node.children) {
        if (child->tagName == tag) {
            return child.get();
        }
        if (const RawNode* deeper = findDescendant(*child, tag)) {
            return deeper;
        }
    }
    return nullptr;
}

void collectDescendants(const RawNode& node, const std::string& tag,
                        std::vector<const RawNode*>& out) {
    for (const auto& child : node.children) {
        if (child->tagName == tag) {
            out.push_back(child.get());
        }
        collectDescendants(*child, tag, out);
    }
}

std::string childText(const RawNode& node, const std::string& tag) {
    const RawNode* child = findChild(node, tag);
    return child == nullptr ? "" : trimText(child->text);
}

std::string nameOf(const RawNode& node) { return childText(node, "SHORT-NAME"); }

// Short names of every `tag` descendant's ref text, in document order.
std::vector<std::string> refNames(const RawNode& node, const std::string& tag) {
    std::vector<const RawNode*> refs;
    collectDescendants(node, tag, refs);
    std::vector<std::string> names;
    names.reserve(refs.size());
    for (const RawNode* ref : refs) {
        names.push_back(shortNameOfRef(ref->text));
    }
    return names;
}

// Whole-string unsigned integer in the given base, as a double.
std::optional<double> parseUnsigned(const std::string& digits, int base) {
    try {
        std::size_t consumed = 0;
        const unsigned long long value = std::stoull(digits, &consumed, base);
        return consumed == digits.size() ? std::optional<double>{static_cast<double>(value)} : std::nullopt;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// AUTOSAR numeric literals may be 0x-hex or 0b-binary (system-4.2 has
// `<LENGTH>0b11</LENGTH>`).
// "0" alone and decimals like "0.1" stay on the stod path.
std::optional<double> parseDouble(const std::string& raw) {
    const std::string text = trimText(raw);
    if (text.empty()) {
        return std::nullopt;
    }
    const bool binary = text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B');
    const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    // The AUTOSAR integer pattern also allows octal: `<LENGTH>013</LENGTH>` is 11.
    const bool octal = text.size() > 1 && text.find_first_not_of("01234567") == std::string::npos &&
                       text[0] == '0';
    if (binary || hex) {
        return parseUnsigned(text.substr(2), binary ? 2 : 16);
    }
    if (octal) {
        return parseUnsigned(text.substr(1), 8);
    }
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        return consumed == text.size() ? std::optional<double>{value} : std::nullopt;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::uint32_t> parseId(const std::string& raw) {
    const std::optional<double> value = parseDouble(raw);
    if (!value || *value < 0.0 || *value > 4294967295.0 || *value != static_cast<std::uint32_t>(*value)) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*value);
}

std::optional<std::int64_t> asInteger(double value) {
    const auto whole = static_cast<std::int64_t>(value);
    return static_cast<double>(whole) == value ? std::optional<std::int64_t>{whole} : std::nullopt;
}

void addUnique(std::vector<std::string>& list, const std::string& name) {
    if (!name.empty() && std::ranges::find(list, name) == list.end()) {
        list.push_back(name);
    }
}

struct FramePort {
    bool out = false;
    std::string ecu;
};

struct Group {
    bool out = false;
    std::vector<std::string> pdus;  // short names
};

// One pass over the raw tree: every lookup the per-frame work needs, so no
// frame ever triggers a tree search. try_emplace keeps the first definition
// when a short name repeats (document order wins).
struct Index {
    std::unordered_map<std::string, const RawNode*> triggerings;  // by FRAME-REF
    std::unordered_map<std::string, const RawNode*> iSignals;
    std::unordered_map<std::string, const RawNode*> systemSignals;
    std::unordered_map<std::string, const RawNode*> compuMethods;
    std::unordered_map<std::string, std::string> units;  // name -> DISPLAY-NAME
    // Keyed by full path: AUTOSAR commonly has RX/<ecu> and TX/<ecu> groups
    // with the same short name, which a short-name join would conflate.
    std::unordered_map<std::string, Group> groupsByPath;
    std::unordered_map<std::string, FramePort> framePorts;
    std::vector<std::pair<std::string, std::vector<std::string>>> ecuGroupRefs;  // ecu, refs
};

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
// Package path and ECU name are both plain strings; one internal recursive caller, order is fixed.
void buildIndex(const RawNode& node, const std::string& pkgPath, const std::string& ecu,
                Index& index) {
    const std::string& tag = node.tagName;
    std::string childPkg = pkgPath;
    std::string childEcu = ecu;
    if (tag == "AR-PACKAGE") {
        childPkg = pkgPath + "/" + nameOf(node);
    } else if (tag == "CAN-FRAME-TRIGGERING") {
        index.triggerings.try_emplace(shortNameOfRef(childText(node, "FRAME-REF")), &node);
    } else if (tag == "I-SIGNAL") {
        index.iSignals.try_emplace(nameOf(node), &node);
    } else if (tag == "SYSTEM-SIGNAL") {
        index.systemSignals.try_emplace(nameOf(node), &node);
    } else if (tag == "COMPU-METHOD") {
        index.compuMethods.try_emplace(nameOf(node), &node);
    } else if (tag == "UNIT") {
        index.units.try_emplace(nameOf(node), childText(node, "DISPLAY-NAME"));
    } else if (tag == "I-SIGNAL-I-PDU-GROUP") {
        Group group;
        group.out = childText(node, "COMMUNICATION-DIRECTION") == "OUT";
        group.pdus = refNames(node, "I-SIGNAL-I-PDU-REF");
        index.groupsByPath.try_emplace(pkgPath + "/" + nameOf(node), std::move(group));
    } else if (tag == "ECU-INSTANCE") {
        childEcu = nameOf(node);
        std::vector<const RawNode*> refs;
        collectDescendants(node, "ASSOCIATED-COM-I-PDU-GROUP-REF", refs);
        std::vector<std::string> paths;
        paths.reserve(refs.size());
        for (const RawNode* ref : refs) {
            paths.push_back(trimText(ref->text));
        }
        index.ecuGroupRefs.emplace_back(childEcu, std::move(paths));
    } else if (tag == "FRAME-PORT") {
        index.framePorts.try_emplace(
            nameOf(node), FramePort{childText(node, "COMMUNICATION-DIRECTION") == "OUT", ecu});
    }
    for (const auto& child : node.children) {
        buildIndex(*child, childPkg, childEcu, index);
    }
}
// NOLINTEND(bugprone-easily-swappable-parameters)

struct Scaling {
    double factor = 1.0;
    double offset = 0.0;
    std::optional<double> min;
    std::optional<double> max;
    std::string unit;
    std::vector<ValueTableEntry> valueTable;
};

std::vector<const RawNode*> childNodes(const RawNode* parent) {
    std::vector<const RawNode*> nodes;
    if (parent == nullptr) {
        return nodes;
    }
    nodes.reserve(parent->children.size());
    for (const auto& value : parent->children) {
        nodes.push_back(value.get());
    }
    return nodes;
}

// [lower, upper] of a linear scale mapped to physical units, when both limits parse.
void applyLimits(const RawNode& scale, Scaling& out) {
    const auto lower = parseDouble(childText(scale, "LOWER-LIMIT"));
    const auto upper = parseDouble(childText(scale, "UPPER-LIMIT"));
    if (!lower || !upper) {
        return;
    }
    double low = *lower * out.factor + out.offset;
    double high = *upper * out.factor + out.offset;
    if (out.factor < 0) {
        std::swap(low, high);
    }
    out.min = low;
    out.max = high;
}

// Takes the scale as the linear (offset, factor) when it has exactly two
// numerators; returns false and leaves `out` untouched otherwise.
bool applyLinearScale(const RawNode& scale, Scaling& out) {
    const RawNode& coeffs = *findChild(scale, "COMPU-RATIONAL-COEFFS");
    const auto numerators = childNodes(findChild(coeffs, "COMPU-NUMERATOR"));
    const auto denominators = childNodes(findChild(coeffs, "COMPU-DENOMINATOR"));
    const std::optional<double> divisor =
        denominators.empty() ? std::optional<double>{1.0} : parseDouble(denominators.front()->text);
    if (numerators.size() != 2 || !divisor || *divisor == 0.0) {
        return false;
    }
    const auto constant = parseDouble(numerators[0]->text);
    const auto slope = parseDouble(numerators[1]->text);
    if (!constant || !slope) {
        return false;
    }
    out.offset = *constant / *divisor;
    out.factor = *slope / *divisor;
    applyLimits(scale, out);
    return true;
}

// A COMPU-CONST/VT scale with lower == upper is one "value = label" row.
void addTextTableEntry(const RawNode& scale, Scaling& out) {
    const RawNode* constant = findChild(scale, "COMPU-CONST");
    const RawNode* label = constant == nullptr ? nullptr : findChild(*constant, "VT");
    const auto lower = parseDouble(childText(scale, "LOWER-LIMIT"));
    const auto upper = parseDouble(childText(scale, "UPPER-LIMIT"));
    if (label == nullptr || !lower || !upper || *lower != *upper) {
        return;
    }
    if (const auto value = asInteger(*lower)) {
        out.valueTable.push_back({*value, trimText(label->text)});
    }
}

// Linear scale and value table from a COMPU-METHOD. Only COMPU-SCALEs with
// exactly two numerators count as linear (offset, factor); others (e.g. a
// rational polynomial) are not representable in DBC and stay at identity.
void applyCompuMethod(const RawNode& method, Scaling& out) {
    std::vector<const RawNode*> scales;
    collectDescendants(method, "COMPU-SCALE", scales);
    bool haveLinear = false;
    for (const RawNode* scale : scales) {
        const RawNode* coeffs = findChild(*scale, "COMPU-RATIONAL-COEFFS");
        if (coeffs != nullptr && !haveLinear) {
            haveLinear = applyLinearScale(*scale, out);
            continue;
        }
        addTextTableEntry(*scale, out);
    }
}

std::string unitName(const Index& index, const RawNode* holder) {
    if (holder == nullptr) {
        return "";
    }
    const RawNode* ref = findDescendant(*holder, "UNIT-REF");
    if (ref == nullptr) {
        return "";
    }
    const auto found = index.units.find(shortNameOfRef(ref->text));
    return found == index.units.end() || found->second == "NoUnit" ? "" : found->second;
}

// I-SIGNAL -> SYSTEM-SIGNAL -> COMPU-METHOD. Returns false when the chain
// does not reach a SYSTEM-SIGNAL, so the caller falls back to the model.
bool resolveScaling(const Index& index, const std::string& signalName, Scaling& out) {
    const auto signalNode = index.iSignals.find(signalName);
    if (signalNode == index.iSignals.end()) {
        return false;
    }
    const RawNode* ref = findChild(*signalNode->second, "SYSTEM-SIGNAL-REF");
    if (ref == nullptr) {
        return false;
    }
    const auto system = index.systemSignals.find(shortNameOfRef(ref->text));
    if (system == index.systemSignals.end()) {
        return false;
    }
    const RawNode* compuRef = findDescendant(*system->second, "COMPU-METHOD-REF");
    const RawNode* method = nullptr;
    if (compuRef != nullptr) {
        const auto found = index.compuMethods.find(shortNameOfRef(compuRef->text));
        if (found != index.compuMethods.end()) {
            method = found->second;
        }
    }
    if (method != nullptr) {
        applyCompuMethod(*method, out);
    }
    out.unit = unitName(index, system->second);
    if (out.unit.empty()) {
        out.unit = unitName(index, method);
    }
    return true;
}

using ModelSignals = std::unordered_map<std::string, std::vector<const Signal*>>;

// The model can hold both an I-SIGNAL and a SYSTEM-SIGNAL under one short
// name; the I-SIGNAL's raw span tells them apart. Without raw data (study
// vocabulary) the one carrying a bit length wins.
const Signal* pickModelSignal(const ModelSignals& modelSignals, const Index& index,
                              const std::string& name) {
    const auto candidates = modelSignals.find(name);
    if (candidates == modelSignals.end()) {
        return nullptr;
    }
    const auto raw = index.iSignals.find(name);
    const Signal* best = nullptr;
    for (const Signal* signal : candidates->second) {
        if (raw != index.iSignals.end() && signal->common.rawSpanRef == raw->second->span) {
            return signal;
        }
        if (best == nullptr || (best->bitLength == 0 && signal->bitLength != 0)) {
            best = signal;
        }
    }
    return best;
}

void warn(ExportDatabase& database, std::string message) {
    Warning warning;
    warning.message = std::move(message);
    database.warnings.push_back(std::move(warning));
}

struct FrameParties {
    std::vector<std::string> transmitters;
    std::vector<std::string> receivers;
};

using PartyMap = std::unordered_map<std::string, FrameParties>;

// FRAME-PORTs on the frame's triggering.
void addPortParties(const ParsedFile& file, const Index& index, PartyMap& parties) {
    for (const Frame& frame : file.frames) {
        const std::string& name = frame.common.shortName;
        const auto trig = index.triggerings.find(name);
        if (trig == index.triggerings.end()) {
            continue;
        }
        for (const std::string& portName : refNames(*trig->second, "FRAME-PORT-REF")) {
            const auto port = index.framePorts.find(portName);
            if (port != index.framePorts.end()) {
                addUnique(port->second.out ? parties[name].transmitters : parties[name].receivers,
                          port->second.ecu);
            }
        }
    }
}

// The ECUs' associated PDU groups: the only source in 4.2-era files.
void addGroupParties(const ParsedFile& file, const Index& index, PartyMap& parties) {
    std::unordered_map<std::string, std::vector<const Frame*>> framesByPdu;
    for (const Frame& frame : file.frames) {
        for (const FramePduMapping& mapping : frame.pdus) {
            framesByPdu[mapping.pduShortNameRef].push_back(&frame);
        }
    }
    for (const auto& [ecu, refs] : index.ecuGroupRefs) {
        for (const std::string& path : refs) {
            const auto group = index.groupsByPath.find(path);
            if (group == index.groupsByPath.end()) {
                continue;
            }
            for (const std::string& pdu : group->second.pdus) {
                for (const Frame* frame : framesByPdu[pdu]) {
                    FrameParties& entry = parties[frame->common.shortName];
                    addUnique(group->second.out ? entry.transmitters : entry.receivers, ecu);
                }
            }
        }
    }
}

// Who sends/receives each frame.
PartyMap resolveParties(const ParsedFile& file, const Index& index) {
    PartyMap parties;
    addPortParties(file, index, parties);
    addGroupParties(file, index, parties);
    return parties;
}

struct Lookups {
    const Index* index;
    const ModelSignals* modelSignals;
    const std::unordered_map<std::string, const Pdu*>* pdus;
};

// CAN id, addressing, FD and transmitters of a frame; nullopt when it has no
// CAN-FRAME-TRIGGERING with a parseable IDENTIFIER.
std::optional<ExportFrame> makeFrameHeader(const Frame& frame, const Index& index,
                                           const FrameParties& who) {
    const auto trig = index.triggerings.find(frame.common.shortName);
    if (trig == index.triggerings.end()) {
        return std::nullopt;
    }
    const RawNode& node = *trig->second;
    const std::optional<std::uint32_t> frameId = parseId(childText(node, "IDENTIFIER"));
    if (!frameId) {
        return std::nullopt;
    }
    ExportFrame out;
    out.name = frame.common.shortName;
    out.id = *frameId;
    out.extended = childText(node, "CAN-ADDRESSING-MODE") == "EXTENDED";
    out.canFd = childText(node, "CAN-FRAME-TX-BEHAVIOR") == "CAN-FD" ||
                childText(node, "CAN-FRAME-RX-BEHAVIOR") == "CAN-FD";
    out.length = frame.length;
    out.transmitters = who.transmitters.empty() ? frame.transmitters : who.transmitters;
    return out;
}

// Scaling from the I-SIGNAL -> SYSTEM-SIGNAL -> COMPU-METHOD chain, else the model's.
void applyScaling(ExportSignal& exported, const Signal& signal, const Index& index) {
    Scaling scaling;
    if (resolveScaling(index, exported.name, scaling)) {
        exported.factor = scaling.factor;
        exported.offset = scaling.offset;
        exported.min = scaling.min;
        exported.max = scaling.max;
        exported.unit = scaling.unit;
        exported.valueTable = std::move(scaling.valueTable);
        return;
    }
    exported.factor = signal.factor;
    exported.offset = signal.offset;
    exported.min = signal.min;
    exported.max = signal.max;
    exported.unit = signal.unit.value_or("");
    exported.valueTable = signal.valueTable.value_or(std::vector<ValueTableEntry>{});
}

ExportSignal makeSignal(const Signal& signal, const PduSignalMapping& place,
                        const FramePduMapping& mapping, const Index& index,
                        const FrameParties& who) {
    ExportSignal exported;
    exported.name = place.signalShortNameRef;
    exported.startBit = place.startPosition + mapping.startPosition;
    exported.byteOrder = place.byteOrder;
    exported.bitLength = signal.bitLength;
    exported.isSigned = signal.isSigned;
    exported.initValue = signal.initValue;
    applyScaling(exported, signal, index);
    // Receivers are frame-wide in 4.2 files (PDU groups carry no
    // per-signal detail); the study vocabulary lists them per signal.
    exported.receivers = who.receivers.empty() ? signal.receivers : who.receivers;
    return exported;
}

// Signals of one PDU placed in the frame; warns and skips what it cannot export.
void appendPduSignals(const Lookups& lookups, const std::string& frameName,
                      const FramePduMapping& mapping, const FrameParties& who, ExportFrame& out,
                      ExportDatabase& database) {
    const auto pduIt = lookups.pdus->find(mapping.pduShortNameRef);
    if (pduIt == lookups.pdus->end()) {
        warn(database, "Frame '" + frameName + "': PDU '" + mapping.pduShortNameRef +
                           "' is not exported (multiplexed, container, secured or unknown PDU)");
        return;
    }
    for (const PduSignalMapping& place : pduIt->second->signalMappings) {
        // An empty ref is an I-SIGNAL-GROUP-REF mapping: groups have no
        // DBC form and their members are mapped one by one anyway.
        if (place.signalShortNameRef.empty()) {
            continue;
        }
        const Signal* signal =
            pickModelSignal(*lookups.modelSignals, *lookups.index, place.signalShortNameRef);
        if (signal == nullptr) {
            warn(database, "Frame '" + frameName + "': signal '" + place.signalShortNameRef +
                               "' is not defined, skipped");
            continue;
        }
        out.signals.push_back(makeSignal(*signal, place, mapping, *lookups.index, who));
    }
}

}  // namespace

ExportDatabase buildExportDatabase(const ParsedFile& file) {
    ExportDatabase database;
    if (!file.clusters.empty()) {
        database.busName = file.clusters.front().common.shortName;
        database.baudrate = file.clusters.front().baudrate;
    }
    for (const EcuInstance& ecu : file.ecuInstances) {
        database.nodes.push_back(ecu.common.shortName);
    }

    Index index;
    if (file.rawDocument != nullptr) {
        buildIndex(file.rawDocument->root, "", "", index);
    }
    ModelSignals modelSignals;
    for (const Signal& signal : file.signals) {
        modelSignals[signal.common.shortName].push_back(&signal);
    }
    std::unordered_map<std::string, const Pdu*> pdus;
    for (const Pdu& pdu : file.pdus) {
        pdus.try_emplace(pdu.common.shortName, &pdu);
    }
    const auto parties = resolveParties(file, index);
    const Lookups lookups{&index, &modelSignals, &pdus};

    for (const Frame& frame : file.frames) {
        const std::string& frameName = frame.common.shortName;
        const auto party = parties.find(frameName);
        const FrameParties none;
        const FrameParties& who = party == parties.end() ? none : party->second;
        std::optional<ExportFrame> out = makeFrameHeader(frame, index, who);
        if (!out) {
            warn(database, "Frame '" + frameName + "' skipped: no CAN-FRAME-TRIGGERING with an IDENTIFIER");
            continue;
        }
        for (const FramePduMapping& mapping : frame.pdus) {
            appendPduSignals(lookups, frameName, mapping, who, *out, database);
        }
        database.frames.push_back(std::move(*out));
    }
    return database;
}
