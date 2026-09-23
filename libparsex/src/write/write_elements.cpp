#include <parsex/write/write_elements.hpp>
#include <parsex/write/write_ordering.hpp>

#include <cstdint>
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

xmlNodePtr buildClusterElement(xmlDocPtr doc, const Cluster& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "CLUSTER");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    appendOptionalTextChild(node, "CATEGORY", value.common.category);
    if (value.baudrate.has_value()) {
        appendTextChild(node, "BAUDRATE", std::to_string(value.baudrate.value()));
    }
    if (!value.physicalChannels.empty()) {
        xmlNodePtr channels = appendChild(node, "PHYSICAL-CHANNELS");
        for (const std::string& channel : sortedStrings(value.physicalChannels)) {
            xmlNodePtr entry = appendChild(channels, "PHYSICAL-CHANNEL");
            appendTextChild(entry, "SHORT-NAME", channel);
        }
    }
    return node;
}

xmlNodePtr buildEcuInstanceElement(xmlDocPtr doc, const EcuInstance& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "ECU-INSTANCE");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    appendOptionalTextChild(node, "CATEGORY", value.common.category);
    if (!value.connectedChannels.empty()) {
        xmlNodePtr channels = appendChild(node, "CONNECTED-CHANNELS");
        for (const std::string& channel : sortedStrings(value.connectedChannels)) {
            xmlNodePtr ref = appendChild(channels, "CHANNEL-REF");
            setSortedAttributes(ref, {{"DEST", "PHYSICAL-CHANNEL"}});
            setNodeText(ref, "/Sys/" + channel);
        }
    }
    if (!value.controllers.empty()) {
        xmlNodePtr controllers = appendChild(node, "CONTROLLERS");
        for (const std::string& controller : sortedStrings(value.controllers)) {
            xmlNodePtr entry = appendChild(controllers, "CAN-CONTROLLER");
            appendTextChild(entry, "SHORT-NAME", controller);
        }
    }
    return node;
}

xmlNodePtr buildFrameElement(xmlDocPtr doc, const Frame& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "FRAME");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    // Numeric fields use plain decimal; precision loss is impossible here
    // (uint32 -> decimal is exact; DLC 64 for CAN FD included).
    appendTextChild(node, "LENGTH", std::to_string(value.length));
    if (!value.transmitters.empty()) {
        xmlNodePtr transmitters = appendChild(node, "TRANSMITTERS");
        for (const std::string& ecu : sortedStrings(value.transmitters)) {
            xmlNodePtr ref = appendChild(transmitters, "TRANSMITTER-REF");
            setSortedAttributes(ref, {{"DEST", "ECU-INSTANCE"}});
            setNodeText(ref, "/Sys/" + ecu);
        }
    }
    if (!value.pdus.empty()) {
        xmlNodePtr pdus = appendChild(node, "PDUS");
        for (const FramePduMapping& mapping : sortedFramePduMappings(value.pdus)) {
            xmlNodePtr entry = appendChild(pdus, "FRAME-PDU");
            // FRAME-PDU carries its own SHORT-NAME in real files; synthesize
            // deterministically (owner + target) — the Parser only reads
            // PDU-REF / START-POSITION, so this never affects round-trip.
            appendTextChild(entry, "SHORT-NAME",
                            value.common.shortName + "_" + mapping.pduShortNameRef);
            xmlNodePtr ref = appendChild(entry, "PDU-REF");
            setSortedAttributes(ref, {{"DEST", "PDU"}});
            setNodeText(ref, "/Sys/" + mapping.pduShortNameRef);
            appendTextChild(entry, "START-POSITION",
                            std::to_string(mapping.startPosition));
        }
    }
    return node;
}

xmlNodePtr buildPduElement(xmlDocPtr doc, const Pdu& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "PDU");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    appendTextChild(node, "LENGTH", std::to_string(value.length));
    if (!value.signalMappings.empty()) {
        xmlNodePtr mappings = appendChild(node, "SIGNAL-MAPPINGS");
        for (const PduSignalMapping& mapping : sortedPduSignalMappings(value.signalMappings)) {
            xmlNodePtr entry = appendChild(mappings, "PDU-SIGNAL-MAPPING");
            appendTextChild(entry, "SHORT-NAME",
                            value.common.shortName + "_" + mapping.signalShortNameRef);
            xmlNodePtr ref = appendChild(entry, "SIGNAL-REF");
            setSortedAttributes(ref, {{"DEST", "SYSTEM-SIGNAL"}});
            setNodeText(ref, "/Sys/" + mapping.signalShortNameRef);
            appendTextChild(entry, "START-POSITION",
                            std::to_string(mapping.startPosition));
            appendTextChild(entry, "BYTE-ORDER", byteOrderToString(mapping.byteOrder));
        }
    }
    return node;
}

xmlNodePtr buildSignalElement(xmlDocPtr doc, const Signal& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "SYSTEM-SIGNAL");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    appendTextChild(node, "START-BIT", std::to_string(value.startBit));
    appendTextChild(node, "BIT-LENGTH", std::to_string(value.bitLength));
    appendTextChild(node, "BYTE-ORDER", byteOrderToString(value.byteOrder));
    if (value.valueTable.has_value()) {
        xmlNodePtr table = appendChild(node, "VALUE-TABLE");
        for (const ValueTableEntry& entry : sortedValueTableEntries(value.valueTable.value())) {
            xmlNodePtr item = appendChild(table, "VALUE-TABLE-ENTRY");
            appendTextChild(item, "VALUE", std::to_string(entry.value));
            appendTextChild(item, "LABEL", entry.label);
        }
    }
    if (!value.receivers.empty()) {
        xmlNodePtr receivers = appendChild(node, "RECEIVERS");
        for (const std::string& ecu : sortedStrings(value.receivers)) {
            xmlNodePtr ref = appendChild(receivers, "RECEIVER-REF");
            setSortedAttributes(ref, {{"DEST", "ECU-INSTANCE"}});
            setNodeText(ref, "/Sys/" + ecu);
        }
    }
    return node;
}

xmlNodePtr buildSignalGroupElement(xmlDocPtr doc, const SignalGroup& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "SIGNAL-GROUP");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    if (!value.members.empty()) {
        xmlNodePtr members = appendChild(node, "MEMBERS");
        for (const std::string& member : sortedStrings(value.members)) {
            // Reference encoding matches the Parser's REF resolution (PAR-94):
            // DEST-typed ref whose text reduces to the short name after the
            // last '/' — reuse exactly, never a new convention.
            xmlNodePtr ref = appendChild(members, "SYSTEM-SIGNAL-REF");
            setSortedAttributes(ref, {{"DEST", "SYSTEM-SIGNAL"}});
            setNodeText(ref, "/Sys/" + member);
        }
    }
    return node;
}
