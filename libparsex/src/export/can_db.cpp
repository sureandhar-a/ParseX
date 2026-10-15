#include <parsex/export/can_db.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
    for (const RawNode* ref : refs) {
        names.push_back(shortNameOfRef(ref->text));
    }
    return names;
}

// AUTOSAR numeric literals may be 0x-hex or 0b-binary (system-4.2 has
// `<LENGTH>0b11</LENGTH>`), which the model builder's parsers reject.
// "0" alone and decimals like "0.1" stay on the strtod path.
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
    char* end = nullptr;
    double value = 0.0;
    if (binary || hex) {
        value = static_cast<double>(std::strtoull(text.c_str() + 2, &end, binary ? 2 : 16));
    } else if (octal) {
        value = static_cast<double>(std::strtoull(text.c_str() + 1, &end, 8));
    } else {
        value = std::strtod(text.c_str(), &end);
    }
    if (end != text.c_str() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint32_t> parseId(const std::string& raw) {
    const std::optional<double> value = parseDouble(raw);
    if (!value || *value < 0.0 || *value > 4294967295.0 || *value != static_cast<std::uint32_t>(*value)) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*value);
}

std::int64_t asInteger(double value, bool& ok) {
    const auto whole = static_cast<std::int64_t>(value);
    ok = static_cast<double>(whole) == value;
    return whole;
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
    // "<pdu>\n<signal>" -> PACKING-BYTE-ORDER is MOST-SIGNIFICANT-BYTE-FIRST.
    std::unordered_map<std::string, bool> bigEndian;
};

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
    } else if (tag == "I-SIGNAL-I-PDU") {
        std::vector<const RawNode*> mappings;
        collectDescendants(node, "I-SIGNAL-TO-I-PDU-MAPPING", mappings);
        for (const RawNode* mapping : mappings) {
            index.bigEndian.try_emplace(
                nameOf(node) + "\n" + shortNameOfRef(childText(*mapping, "I-SIGNAL-REF")),
                childText(*mapping, "PACKING-BYTE-ORDER") == "MOST-SIGNIFICANT-BYTE-FIRST");
        }
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

struct Scaling {
    double factor = 1.0;
    double offset = 0.0;
    std::optional<double> min;
    std::optional<double> max;
    std::string unit;
    std::vector<ValueTableEntry> valueTable;
};

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
            std::vector<const RawNode*> numerators;
            std::vector<const RawNode*> denominators;
            if (const RawNode* num = findChild(*coeffs, "COMPU-NUMERATOR")) {
                for (const auto& value : num->children) {
                    numerators.push_back(value.get());
                }
            }
            if (const RawNode* den = findChild(*coeffs, "COMPU-DENOMINATOR")) {
                for (const auto& value : den->children) {
                    denominators.push_back(value.get());
                }
            }
            const std::optional<double> divisor =
                denominators.empty() ? std::optional<double>{1.0} : parseDouble(denominators[0]->text);
            if (numerators.size() == 2 && divisor && *divisor != 0.0) {
                const auto constant = parseDouble(numerators[0]->text);
                const auto slope = parseDouble(numerators[1]->text);
                if (constant && slope) {
                    haveLinear = true;
                    out.offset = *constant / *divisor;
                    out.factor = *slope / *divisor;
                    const auto lower = parseDouble(childText(*scale, "LOWER-LIMIT"));
                    const auto upper = parseDouble(childText(*scale, "UPPER-LIMIT"));
                    if (lower && upper) {
                        double lo = *lower * out.factor + out.offset;
                        double hi = *upper * out.factor + out.offset;
                        if (out.factor < 0) {
                            std::swap(lo, hi);
                        }
                        out.min = lo;
                        out.max = hi;
                    }
                }
            }
            continue;
        }
        const RawNode* constant = findChild(*scale, "COMPU-CONST");
        const RawNode* label = constant == nullptr ? nullptr : findChild(*constant, "VT");
        const auto lower = parseDouble(childText(*scale, "LOWER-LIMIT"));
        const auto upper = parseDouble(childText(*scale, "UPPER-LIMIT"));
        if (label != nullptr && lower && upper && *lower == *upper) {
            bool whole = false;
            const std::int64_t value = asInteger(*lower, whole);
            if (whole) {
                out.valueTable.push_back({value, trimText(label->text)});
            }
        }
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

void warn(ExportDatabase& db, std::string message) {
    Warning warning;
    warning.message = std::move(message);
    db.warnings.push_back(std::move(warning));
}

struct FrameParties {
    std::vector<std::string> transmitters;
    std::vector<std::string> receivers;
};

// Who sends/receives each frame, from FRAME-PORTs on the triggering and from
// the ECUs' associated PDU groups (the only source in 4.2-era files).
std::unordered_map<std::string, FrameParties> resolveParties(const ParsedFile& file,
                                                             const Index& index) {
    std::unordered_map<std::string, FrameParties> parties;
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
    std::unordered_map<std::string, std::vector<const Frame*>> framesByPdu;
    for (const Frame& frame : file.frames) {
        for (const FramePduMapping& mapping : frame.pdus) {
            framesByPdu[mapping.pduShortNameRef].push_back(&frame);
        }
    }
    for (const auto& [ecu, refs] : index.ecuGroupRefs) {
        for (const std::string& path : refs) {
            auto group = index.groupsByPath.find(path);
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
    return parties;
}

}  // namespace

ExportDatabase buildExportDatabase(const ParsedFile& file) {
    ExportDatabase db;
    if (!file.clusters.empty()) {
        db.busName = file.clusters.front().common.shortName;
        db.baudrate = file.clusters.front().baudrate;
    }
    for (const EcuInstance& ecu : file.ecuInstances) {
        db.nodes.push_back(ecu.common.shortName);
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

    for (const Frame& frame : file.frames) {
        const std::string& frameName = frame.common.shortName;
        const auto trig = index.triggerings.find(frameName);
        const std::optional<std::uint32_t> id =
            trig == index.triggerings.end()
                ? std::nullopt
                : parseId(childText(*trig->second, "IDENTIFIER"));
        if (!id) {
            warn(db, "Frame '" + frameName + "' skipped: no CAN-FRAME-TRIGGERING with an IDENTIFIER");
            continue;
        }

        ExportFrame out;
        out.name = frameName;
        out.id = *id;
        out.extended = childText(*trig->second, "CAN-ADDRESSING-MODE") == "EXTENDED";
        out.canFd = childText(*trig->second, "CAN-FRAME-TX-BEHAVIOR") == "CAN-FD" ||
                    childText(*trig->second, "CAN-FRAME-RX-BEHAVIOR") == "CAN-FD";
        out.length = frame.length;

        const auto party = parties.find(frameName);
        const FrameParties none;
        const FrameParties& who = party == parties.end() ? none : party->second;
        out.transmitters = who.transmitters;
        if (out.transmitters.empty()) {
            out.transmitters = frame.transmitters;
        }

        for (const FramePduMapping& mapping : frame.pdus) {
            const auto pduIt = pdus.find(mapping.pduShortNameRef);
            if (pduIt == pdus.end()) {
                warn(db, "Frame '" + frameName + "': PDU '" + mapping.pduShortNameRef +
                             "' is not exported (multiplexed, container, secured or unknown PDU)");
                continue;
            }
            for (const PduSignalMapping& place : pduIt->second->signalMappings) {
                const Signal* signalPtr = pickModelSignal(modelSignals, index, place.signalShortNameRef);
                if (signalPtr == nullptr) {
                    warn(db, "Frame '" + frameName + "': signal '" + place.signalShortNameRef +
                                 "' is not defined, skipped");
                    continue;
                }
                const Signal& signal = *signalPtr;
                ExportSignal exported;
                exported.name = place.signalShortNameRef;
                exported.startBit = place.startPosition + mapping.startPosition;
                exported.byteOrder = place.byteOrder;
                exported.bitLength = signal.bitLength;
                exported.isSigned = signal.isSigned;
                exported.initValue = signal.initValue;
                // The model folds MOST-SIGNIFICANT-BYTE-LAST (little endian)
                // into big endian and drops 0b-literals; the raw nodes are
                // authoritative whenever they exist.
                const auto packing = index.bigEndian.find(pduIt->first + "\n" + exported.name);
                if (packing != index.bigEndian.end()) {
                    exported.byteOrder = packing->second ? ByteOrder::MostSignificantByteFirst
                                                         : ByteOrder::LeastSignificantByteFirst;
                }
                if (const auto raw = index.iSignals.find(exported.name); raw != index.iSignals.end()) {
                    if (const auto length = parseDouble(childText(*raw->second, "LENGTH"))) {
                        exported.bitLength = static_cast<std::uint32_t>(*length);
                    }
                    const RawNode* init = findChild(*raw->second, "INIT-VALUE");
                    const RawNode* value = init == nullptr ? nullptr : findDescendant(*init, "VALUE");
                    if (value != nullptr) {
                        exported.initValue = parseDouble(value->text);
                    }
                }

                Scaling scaling;
                if (resolveScaling(index, exported.name, scaling)) {
                    exported.factor = scaling.factor;
                    exported.offset = scaling.offset;
                    exported.min = scaling.min;
                    exported.max = scaling.max;
                    exported.unit = scaling.unit;
                    exported.valueTable = std::move(scaling.valueTable);
                } else {
                    exported.factor = signal.factor;
                    exported.offset = signal.offset;
                    exported.min = signal.min;
                    exported.max = signal.max;
                    exported.unit = signal.unit.value_or("");
                    exported.valueTable = signal.valueTable.value_or(std::vector<ValueTableEntry>{});
                }
                // Receivers are frame-wide in 4.2 files (PDU groups carry no
                // per-signal detail); the study vocabulary lists them per signal.
                exported.receivers = who.receivers.empty() ? signal.receivers : who.receivers;
                out.signals.push_back(std::move(exported));
            }
        }
        db.frames.push_back(std::move(out));
    }
    return db;
}
