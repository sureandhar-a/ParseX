#include <parsex/write/write_elements.hpp>
#include <parsex/write/write_ordering.hpp>

#include <cstdint>
#include <sstream>
#include <string>

xmlNodePtr appendChild(xmlNodePtr parent, const char* tagName) {
    return xmlNewChild(parent, nullptr, BAD_CAST tagName, nullptr);
}

void setNodeText(xmlNodePtr node, const std::string& text) {
    xmlNodeSetContent(node, BAD_CAST text.c_str());
}

void appendTextChild(xmlNodePtr parent, const char* tagName, const std::string& text) {
    xmlNodePtr child = xmlNewChild(parent, nullptr, BAD_CAST tagName, nullptr);
    if (child != nullptr) {
        xmlNodeSetContent(child, BAD_CAST text.c_str());
    }
}

void appendOptionalTextChild(xmlNodePtr parent, const char* tagName,
                             const std::optional<std::string>& value) {
    // Optional fields are only written when present: no empty placeholders.
    if (value.has_value()) {
        appendTextChild(parent, tagName, value.value());
    }
}

std::string byteOrderToString(ByteOrder order) {
    return (order == ByteOrder::LeastSignificantByteFirst)
               ? "LEAST-SIGNIFICANT-BYTE-FIRST"
               : "MOST-SIGNIFICANT-BYTE-FIRST";
}

namespace {

// Decimal rendering with enough digits to reparse to the identical double
// (matches diffValueToString(double)'s precision so round-trip diffs compare
// equal whenever the numeric values are equal).
std::string doubleToString(double value) {
    std::ostringstream out;
    out.precision(17);
    out << value;
    return out.str();
}

}  // namespace

// Real-vocabulary builders (post-pivot): every element/tag below is a genuine
// AUTOSAR 4.x element (verified against resources/schemas/4.4.0.xsd and
// tests/fixtures/system-4.2.arxml), so WriteEngine output validates with
// xmllint --schema. Tags mirror exactly what the Parser reads back
// (see model_builder.hpp's real-vocabulary notes), keeping round-trip exact
// for everything the domain model represents in real-vocabulary terms.
//
// Known v1 scope limits (no single-node schema-valid home exists; each is a
// faithful round-trip loss only for study-vocabulary inputs that populate
// these fields — real-vocabulary fixtures leave them empty and round-trip
// exactly):
// - EcuInstance.connectedChannels: dropped (real ECU-INSTANCE carries no
//   channel refs; connectivity lives in PDU-TRIGGERINGs outside one node).
// - Frame.transmitters: dropped (senders live behind FRAME-TRIGGERINGs).
// - Signal.startBit when non-zero: dropped (packing lives on the owning
//   PDU's I-SIGNAL-TO-I-PDU-MAPPING, captured in PduSignalMapping instead).
// - Signal byteOrder when non-default: PACKING-BYTE-ORDER omitted (both the
//   real LAST-variant and absence map to the domain default).
// - Signal.receivers: dropped (no per-I-SIGNAL receiver list in the schema).
// - Signal.valueTable: dropped (real value tables live in COMPU-METHODs,
//   separate elements out of v1 scope).

xmlNodePtr buildClusterElement(xmlDocPtr doc, const Cluster& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "CAN-CLUSTER");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    appendOptionalTextChild(node, "CATEGORY", value.common.category);
    if (value.baudrate.has_value() || !value.physicalChannels.empty()) {
        xmlNodePtr variants = appendChild(node, "CAN-CLUSTER-VARIANTS");
        xmlNodePtr conditional = appendChild(variants, "CAN-CLUSTER-CONDITIONAL");
        if (value.baudrate.has_value()) {
            appendTextChild(conditional, "BAUDRATE", std::to_string(value.baudrate.value()));
        }
        if (!value.physicalChannels.empty()) {
            xmlNodePtr channels = appendChild(conditional, "PHYSICAL-CHANNELS");
            for (const std::string& channel : sortedStrings(value.physicalChannels)) {
                xmlNodePtr entry = appendChild(channels, "CAN-PHYSICAL-CHANNEL");
                appendTextChild(entry, "SHORT-NAME", channel);
            }
        }
    }
    return node;
}

xmlNodePtr buildEcuInstanceElement(xmlDocPtr doc, const EcuInstance& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "ECU-INSTANCE");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    appendOptionalTextChild(node, "CATEGORY", value.common.category);
    if (!value.controllers.empty()) {
        xmlNodePtr controllers = appendChild(node, "COMM-CONTROLLERS");
        for (const std::string& controller : sortedStrings(value.controllers)) {
            xmlNodePtr entry = appendChild(controllers, "CAN-COMMUNICATION-CONTROLLER");
            appendTextChild(entry, "SHORT-NAME", controller);
        }
    }
    return node;
}

xmlNodePtr buildFrameElement(xmlDocPtr doc, const Frame& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "CAN-FRAME");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    // Numeric fields use plain decimal; uint32 -> decimal is exact.
    appendTextChild(node, "FRAME-LENGTH", std::to_string(value.length));
    if (!value.pdus.empty()) {
        xmlNodePtr mappings = appendChild(node, "PDU-TO-FRAME-MAPPINGS");
        const std::vector<FramePduMapping> sorted = sortedFramePduMappings(value.pdus);
        for (std::size_t idx = 0; idx < sorted.size(); ++idx) {
            const FramePduMapping& mapping = sorted.at(idx);
            xmlNodePtr entry = appendChild(mappings, "PDU-TO-FRAME-MAPPING");
            // Mappings carry their own SHORT-NAME in real files; synthesize
            // deterministically (owner + target, or owner + index when the
            // target is outside the domain) — the Parser only reads
            // PDU-REF / START-POSITION, so this never affects round-trip.
            appendTextChild(entry, "SHORT-NAME",
                            mapping.pduShortNameRef.empty()
                                ? value.common.shortName + "_mapping_" + std::to_string(idx)
                                : value.common.shortName + "_" + mapping.pduShortNameRef);
            if (!mapping.pduShortNameRef.empty()) {
                xmlNodePtr ref = appendChild(entry, "PDU-REF");
                setSortedAttributes(ref, {{"DEST", "I-SIGNAL-I-PDU"}});
                setNodeText(ref, "/Sys/" + mapping.pduShortNameRef);
            }
            // An empty ref means the original mapping targeted something
            // outside the six-type domain (e.g. a MULTIPLEXED-I-PDU via a
            // differently-typed REF the Parser ignores): omitting the REF
            // child is schema-valid (both REF alternatives are minOccurs=0)
            // and reads back as empty, preserving round-trip.
            appendTextChild(entry, "START-POSITION",
                            std::to_string(mapping.startPosition));
        }
    }
    return node;
}

xmlNodePtr buildPduElement(xmlDocPtr doc, const Pdu& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "I-SIGNAL-I-PDU");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    appendTextChild(node, "LENGTH", std::to_string(value.length));
    if (!value.signalMappings.empty()) {
        xmlNodePtr mappings = appendChild(node, "I-SIGNAL-TO-PDU-MAPPINGS");
        const std::vector<PduSignalMapping> sorted =
            sortedPduSignalMappings(value.signalMappings);
        for (std::size_t idx = 0; idx < sorted.size(); ++idx) {
            const PduSignalMapping& mapping = sorted.at(idx);
            xmlNodePtr entry = appendChild(mappings, "I-SIGNAL-TO-I-PDU-MAPPING");
            appendTextChild(entry, "SHORT-NAME",
                            mapping.signalShortNameRef.empty()
                                ? value.common.shortName + "_mapping_" + std::to_string(idx)
                                : value.common.shortName + "_" + mapping.signalShortNameRef);
            if (!mapping.signalShortNameRef.empty()) {
                xmlNodePtr ref = appendChild(entry, "I-SIGNAL-REF");
                setSortedAttributes(ref, {{"DEST", "I-SIGNAL"}});
                setNodeText(ref, "/Sys/" + mapping.signalShortNameRef);
            }
            // An empty ref means the original mapping targeted an
            // I-SIGNAL-GROUP (I-SIGNAL-GROUP-REF, outside the six-type
            // domain — cf. system-4.2.arxml's message1/message3 PDUs):
            // omitting I-SIGNAL-REF is schema-valid (both REF alternatives
            // are minOccurs=0 and mutually exclusive) and reads back as
            // empty, preserving round-trip. Group identity itself is out of
            // v1 scope.
            appendTextChild(entry, "START-POSITION",
                            std::to_string(mapping.startPosition));
        }
    }
    return node;
}

xmlNodePtr buildSignalElement(xmlDocPtr doc, const Signal& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "I-SIGNAL");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    if (value.initValue.has_value()) {
        xmlNodePtr init = appendChild(node, "INIT-VALUE");
        xmlNodePtr spec = appendChild(init, "NUMERICAL-VALUE-SPECIFICATION");
        appendTextChild(spec, "VALUE", doubleToString(value.initValue.value()));
    }
    appendTextChild(node, "LENGTH", std::to_string(value.bitLength));
    if (value.isSigned) {
        // Signedness round-trips via a BASE-TYPE-REF whose short name matches
        // the Parser's S(INT|[0-9]) heuristic (see model_builder.cpp) —
        // nested exactly as in real files (cf. system-4.2.arxml).
        xmlNodePtr netProps = appendChild(node, "NETWORK-REPRESENTATION-PROPS");
        xmlNodePtr variants = appendChild(netProps, "SW-DATA-DEF-PROPS-VARIANTS");
        xmlNodePtr conditional = appendChild(variants, "SW-DATA-DEF-PROPS-CONDITIONAL");
        xmlNodePtr ref = appendChild(conditional, "BASE-TYPE-REF");
        setSortedAttributes(ref, {{"DEST", "SW-BASE-TYPE"}});
        setNodeText(ref, "/SwBaseType/SINT8");
    }
    return node;
}

xmlNodePtr buildSignalGroupElement(xmlDocPtr doc, const SignalGroup& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "I-SIGNAL-GROUP");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    if (!value.members.empty()) {
        xmlNodePtr members = appendChild(node, "I-SIGNAL-REFS");
        for (const std::string& member : sortedStrings(value.members)) {
            // Reference encoding matches the Parser's REF resolution (PAR-94):
            // DEST-typed ref whose text reduces to the short name after the
            // last '/' — reuse exactly, never a new convention.
            xmlNodePtr ref = appendChild(members, "I-SIGNAL-REF");
            setSortedAttributes(ref, {{"DEST", "I-SIGNAL"}});
            setNodeText(ref, "/Sys/" + member);
        }
    }
    return node;
}
