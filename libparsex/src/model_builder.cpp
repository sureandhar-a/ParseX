#include <parsex/parser/model_builder.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <string>
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

std::string asciiUpperCopy(std::string text) {
    for (char& letter : text) {
        if (letter >= 'a' && letter <= 'z') {
            letter = static_cast<char>(letter - ('a' - 'A'));
        }
    }
    return text;
}

bool isHexPrefixed(const std::string& text) {
    const std::size_t digits =
        (text.at(0) == '+' || text.at(0) == '-') ? std::size_t{1} : std::size_t{0};
    return text.compare(digits, 2, "0x") == 0 || text.compare(digits, 2, "0X") == 0;
}

const RawNode* findChild(const RawNode& node, const std::string& tag) {
    for (const auto& child : node.children) {
        if (child->tagName == tag) {
            return child.get();
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

std::vector<const RawNode*> findDescendants(const RawNode& node, const std::string& tag) {
    std::vector<const RawNode*> found;
    collectDescendants(node, tag, found);
    return found;
}

const RawNode* findDescendant(const RawNode& node, const std::string& tag) {
    const std::vector<const RawNode*> found = findDescendants(node, tag);
    return found.empty() ? nullptr : found.front();
}

// Trimmed text of a direct child, or nullopt when the child is absent.
// Present-but-blank yields "" (element exists, value empty).
std::optional<std::string> childText(const RawNode& node, const std::string& tag) {
    const RawNode* child = findChild(node, tag);
    if (child == nullptr) {
        return std::nullopt;
    }
    return trimText(child->text);
}

std::optional<std::int64_t> parseInteger(const std::string& raw) {
    const std::string text = trimText(raw);
    if (text.empty()) {
        return std::nullopt;
    }
    try {
        std::size_t pos = 0;
        bool negative = false;
        if (text.at(0) == '+' || text.at(0) == '-') {
            negative = text.at(0) == '-';
            pos = 1;
        }
        unsigned long long magnitude = 0;
        if (isHexPrefixed(text)) {
            magnitude = std::stoull(text.substr(pos + 2), nullptr, 16);
        } else {
            const long long signedValue = std::stoll(text.substr(pos));
            if (negative) {
                return -signedValue;
            }
            magnitude = static_cast<unsigned long long>(signedValue);
        }
        if (negative) {
            return -static_cast<std::int64_t>(magnitude);
        }
        if (magnitude > static_cast<unsigned long long>(std::numeric_limits<std::int64_t>::max())) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(magnitude);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::uint32_t> parseUint32(const std::string& raw) {
    const std::optional<std::int64_t> value = parseInteger(raw);
    if (!value.has_value()) {
        return std::nullopt;
    }
    constexpr std::int64_t maxUint32 = std::numeric_limits<std::uint32_t>::max();
    if (value.value() < 0 || value.value() > maxUint32) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value.value());
}

std::optional<double> parseDouble(const std::string& raw) {
    const std::string text = trimText(raw);
    if (text.empty()) {
        return std::nullopt;
    }
    try {
        if (isHexPrefixed(text)) {
            const std::optional<std::int64_t> asInt = parseInteger(text);
            if (!asInt.has_value()) {
                return std::nullopt;
            }
            return static_cast<double>(asInt.value());
        }
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size()) {
            return std::nullopt;
        }
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// Reference-valued strings reduce to short names (text after the last '/').
std::string shortNameOfRef(const std::string& ref) {
    std::string text = trimText(ref);
    const std::size_t slash = text.rfind('/');
    if (slash == std::string::npos) {
        return text;
    }
    return text.substr(slash + 1);
}

ByteOrder parseByteOrder(const std::string& raw) {
    const std::string text = asciiUpperCopy(trimText(raw));
    if (text == "LEAST-SIGNIFICANT-BYTE-FIRST") {
        return ByteOrder::LeastSignificantByteFirst;
    }
    // Anything else (including MOST-SIGNIFICANT-BYTE-LAST, which the domain
    // cannot express) falls back to the domain default — see header.
    return ByteOrder::MostSignificantByteFirst;
}

// Signed iff the base-type short name looks like S16 / SINT8 / ... — the
// authoritative BASE-TYPE-ENCODING is unreachable single-node (see header).
bool isSignedBaseType(const std::string& baseTypeRef) {
    const std::string name = asciiUpperCopy(shortNameOfRef(baseTypeRef));
    if (name.starts_with("SINT")) {
        return true;
    }
    return name.size() >= 2 && name.at(0) == 'S' && name.at(1) >= '0' && name.at(1) <= '9';
}

// Universal AUTOSAR convention: essentially every named element carries its
// name in a SHORT-NAME child. Missing (malformed input) yields "" —
// best-effort parsing leaves conformance to the Validator.
std::string extractShortName(const RawNode& node) {
    const std::optional<std::string> shortName = childText(node, "SHORT-NAME");
    return shortName.has_value() ? shortName.value() : "";
}

// Single shared population point for the composed CommonFields block (same
// "one place to change" reasoning as CommonFields itself being shared).
CommonFields populateCommonFields(const RawNode& node, std::vector<Warning>& warnings) {
    CommonFields common;
    common.shortName = extractShortName(node);
    if (common.shortName.empty()) {
        // SHORT-NAME is required on every named AUTOSAR element; without it
        // every message below would quote an empty name, so flag it here once.
        Warning warning;
        warning.message = "<" + node.tagName + "> element has no SHORT-NAME";
        warning.location = node.span;
        warnings.push_back(std::move(warning));
    }
    const std::optional<std::string> category = childText(node, "CATEGORY");
    if (category.has_value()) {
        common.category = category;
    }
    // No XML reading here: the Loader already computed this span, and this
    // copy is the back-reference the Write Engine will splice through later.
    common.rawSpanRef = node.span;
    return common;
}

void warnMissing(std::vector<Warning>& warnings, std::string message, RawSpan span) {
    Warning warning;
    warning.message = std::move(message);
    warning.location = span;
    warnings.push_back(std::move(warning));
}

// Trimmed texts of `refTag` descendants (optionally scoped under a wrapper):
// e.g. TRANSMITTER-REF texts under TRANSMITTERS, or bare descendants.
std::vector<std::string> refShortNames(const RawNode& node, const std::string& refTag) {
    std::vector<std::string> names;
    for (const RawNode* ref : findDescendants(node, refTag)) {
        const std::string name = shortNameOfRef(ref->text);
        if (!name.empty()) {
            names.push_back(name);
        }
    }
    return names;
}

// Direct SHORT-NAME text of every descendant element matching channelTag
// (PHYSICAL-CHANNEL, CAN-PHYSICAL-CHANNEL, ...) — direct child only, so a
// channel's own nested SHORT-NAMEs (triggerings, ...) never leak in.
std::vector<std::string> channelShortNames(const RawNode& node) {
    std::vector<std::string> names;
    for (const auto& tags : {findDescendants(node, "PHYSICAL-CHANNEL"),
                             findDescendants(node, "CAN-PHYSICAL-CHANNEL"),}) {
        for (const RawNode* channel : tags) {
            const std::optional<std::string> name = childText(*channel, "SHORT-NAME");
            if (name.has_value() && !name.value().empty()) {
                names.push_back(name.value());
            }
        }
    }
    return names;
}

std::vector<std::string> controllerShortNames(const RawNode& node) {
    std::vector<std::string> names;
    for (const auto& tags : {findDescendants(node, "CAN-CONTROLLER"),
                             findDescendants(node, "CAN-COMMUNICATION-CONTROLLER"),}) {
        for (const RawNode* controller : tags) {
            const std::optional<std::string> name = childText(*controller, "SHORT-NAME");
            if (name.has_value() && !name.value().empty()) {
                names.push_back(name.value());
            }
        }
    }
    return names;
}

std::uint32_t childUint32(const RawNode& node, const std::string& tag, std::uint32_t fallback) {
    const std::optional<std::string> text = childText(node, tag);
    if (!text.has_value()) {
        return fallback;
    }
    const std::optional<std::uint32_t> value = parseUint32(text.value());
    return value.has_value() ? value.value() : fallback;
}

}  // namespace

Cluster buildCluster(const RawNode& node, std::vector<Warning>& warnings) {
    Cluster cluster;
    cluster.common = populateCommonFields(node, warnings);
    // Flat BAUDRATE (study vocabulary) or nested under CAN-CLUSTER-VARIANTS /
    // CAN-CLUSTER-CONDITIONAL (real vocabulary) — first hit wins.
    std::optional<std::string> baudrate = childText(node, "BAUDRATE");
    if (!baudrate.has_value()) {
        const RawNode* found = findDescendant(node, "BAUDRATE");
        if (found != nullptr) {
            baudrate = trimText(found->text);
        }
    }
    if (!baudrate.has_value()) {
        warnMissing(warnings, "Cluster '" + cluster.common.shortName + "' has no baudrate",
                    node.span);
    } else {
        cluster.baudrate = parseUint32(baudrate.value());
    }
    cluster.physicalChannels = channelShortNames(node);
    return cluster;
}

EcuInstance buildEcuInstance(const RawNode& node, std::vector<Warning>& warnings) {
    EcuInstance ecu;
    ecu.common = populateCommonFields(node, warnings);
    ecu.connectedChannels = refShortNames(node, "CHANNEL-REF");
    ecu.controllers = controllerShortNames(node);
    if (ecu.controllers.empty() && ecu.connectedChannels.empty()) {
        warnMissing(warnings,
                    "EcuInstance '" + ecu.common.shortName +
                        "' has no controllers and no connected channels",
                    node.span);
    }
    return ecu;
}

Frame buildFrame(const RawNode& node, std::vector<Warning>& warnings) {
    Frame frame;
    frame.common = populateCommonFields(node, warnings);
    const bool hasLength =
        findChild(node, "LENGTH") != nullptr || findChild(node, "FRAME-LENGTH") != nullptr;
    if (!hasLength) {
        warnMissing(warnings, "Frame '" + frame.common.shortName + "' has no length",
                    node.span);
    }
    frame.length = childUint32(node, "LENGTH", childUint32(node, "FRAME-LENGTH", 0));
    frame.transmitters = refShortNames(node, "TRANSMITTER-REF");
    for (const auto& tags : {findDescendants(node, "FRAME-PDU"),
                             findDescendants(node, "PDU-TO-FRAME-MAPPING"),}) {
        for (const RawNode* mapping : tags) {
            FramePduMapping entry;
            const std::optional<std::string> ref = childText(*mapping, "PDU-REF");
            entry.pduShortNameRef = ref.has_value() ? shortNameOfRef(ref.value()) : "";
            entry.startPosition = childUint32(*mapping, "START-POSITION", 0);
            frame.pdus.push_back(std::move(entry));
        }
    }
    if (frame.pdus.empty()) {
        warnMissing(warnings, "Frame '" + frame.common.shortName + "' has no PDU mappings",
                    node.span);
    }
    return frame;
}

Pdu buildPdu(const RawNode& node, std::vector<Warning>& warnings) {
    Pdu pdu;
    pdu.common = populateCommonFields(node, warnings);
    if (findChild(node, "LENGTH") == nullptr) {
        warnMissing(warnings, "Pdu '" + pdu.common.shortName + "' has no length", node.span);
    }
    pdu.length = childUint32(node, "LENGTH", 0);
    for (const auto& tags : {findDescendants(node, "PDU-SIGNAL-MAPPING"),
                             findDescendants(node, "I-SIGNAL-TO-I-PDU-MAPPING"),}) {
        for (const RawNode* mapping : tags) {
            PduSignalMapping entry;
            const std::optional<std::string> signalRef =
                childText(*mapping, "SIGNAL-REF").has_value()
                    ? childText(*mapping, "SIGNAL-REF")
                    : childText(*mapping, "I-SIGNAL-REF");
            entry.signalShortNameRef =
                signalRef.has_value() ? shortNameOfRef(signalRef.value()) : "";
            entry.startPosition = childUint32(*mapping, "START-POSITION", 0);
            const std::optional<std::string> order =
                childText(*mapping, "BYTE-ORDER").has_value()
                    ? childText(*mapping, "BYTE-ORDER")
                    : childText(*mapping, "PACKING-BYTE-ORDER");
            if (order.has_value()) {
                entry.byteOrder = parseByteOrder(order.value());
            }
            pdu.signalMappings.push_back(std::move(entry));
        }
    }
    if (pdu.signalMappings.empty()) {
        warnMissing(warnings, "Pdu '" + pdu.common.shortName + "' maps no signals",
                    node.span);
    }
    return pdu;
}

Signal buildSignal(const RawNode& node, std::vector<Warning>& warnings) {
    Signal signal;
    signal.common = populateCommonFields(node, warnings);
    signal.startBit = childUint32(node, "START-BIT", 0);
    // Only I-SIGNAL owns its length: LENGTH is schema-required there, so its
    // absence is always worth flagging. A SYSTEM-SIGNAL's length lives on its
    // I-SIGNAL instead — absence here is normal (even bare stubs stay silent),
    // so the tag gate is the whole rule.
    if (node.tagName == "I-SIGNAL" && findChild(node, "LENGTH") == nullptr) {
        warnMissing(warnings, "Signal '" + signal.common.shortName + "' has no bit length",
                    node.span);
    }
    signal.bitLength = childUint32(node, "BIT-LENGTH", childUint32(node, "LENGTH", 0));
    const std::optional<std::string> order = childText(node, "BYTE-ORDER");
    if (order.has_value()) {
        signal.byteOrder = parseByteOrder(order.value());
    }
    const RawNode* baseTypeRef = findDescendant(node, "BASE-TYPE-REF");
    if (baseTypeRef != nullptr) {
        signal.isSigned = isSignedBaseType(baseTypeRef->text);
    }
    // INIT-VALUE is scoped to its own subtree so VALUE-TABLE VALUEs (a
    // SYSTEM-SIGNAL shape) can never leak in here, and vice versa below.
    const RawNode* initValue = findChild(node, "INIT-VALUE");
    if (initValue != nullptr) {
        const RawNode* value = findDescendant(*initValue, "VALUE");
        if (value != nullptr) {
            signal.initValue = parseDouble(value->text);
        }
    }
    signal.receivers = refShortNames(node, "RECEIVER-REF");
    const RawNode* valueTable = findChild(node, "VALUE-TABLE");
    if (valueTable != nullptr) {
        std::vector<ValueTableEntry> entries;
        for (const auto& child : valueTable->children) {
            if (child->tagName != "VALUE-TABLE-ENTRY") {
                continue;
            }
            ValueTableEntry entry;
            const std::optional<std::string> valueText = childText(*child, "VALUE");
            if (valueText.has_value()) {
                const std::optional<std::int64_t> parsed = parseInteger(valueText.value());
                entry.value = parsed.has_value() ? parsed.value() : 0;
            }
            const std::optional<std::string> label = childText(*child, "LABEL");
            entry.label = label.has_value() ? label.value() : "";
            entries.push_back(std::move(entry));
        }
        signal.valueTable = std::move(entries);
    }
    return signal;
}

SignalGroup buildSignalGroup(const RawNode& node, std::vector<Warning>& warnings) {
    SignalGroup group;
    group.common = populateCommonFields(node, warnings);
    // Real vocabulary references I-SIGNALs; the study vocabulary references
    // SYSTEM-SIGNALs — both reduce to short names uniformly.
    std::vector<std::string> members = refShortNames(node, "I-SIGNAL-REF");
    const std::vector<std::string> systemRefs = refShortNames(node, "SYSTEM-SIGNAL-REF");
    members.insert(members.end(), systemRefs.begin(), systemRefs.end());
    group.members = std::move(members);
    if (group.members.empty()) {
        warnMissing(warnings, "SignalGroup '" + group.common.shortName + "' has no members",
                    node.span);
    }
    return group;
}
