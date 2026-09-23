#include <parsex/write/write_elements.hpp>

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
        for (const std::string& channel : value.physicalChannels) {
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
        for (const std::string& channel : value.connectedChannels) {
            xmlNodePtr ref = appendChild(channels, "CHANNEL-REF");
            xmlNewProp(ref, BAD_CAST "DEST", BAD_CAST "PHYSICAL-CHANNEL");
            setNodeText(ref, "/Sys/" + channel);
        }
    }
    if (!value.controllers.empty()) {
        xmlNodePtr controllers = appendChild(node, "CONTROLLERS");
        for (const std::string& controller : value.controllers) {
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
        for (const std::string& ecu : value.transmitters) {
            xmlNodePtr ref = appendChild(transmitters, "TRANSMITTER-REF");
            xmlNewProp(ref, BAD_CAST "DEST", BAD_CAST "ECU-INSTANCE");
            setNodeText(ref, "/Sys/" + ecu);
        }
    }
    // TODO(PAR-146): Frame PDU-triggering mappings (FRAME-PDU / PDU-REF /
    // START-POSITION) — nested-collection structure goes here.
    return node;
}

xmlNodePtr buildPduElement(xmlDocPtr doc, const Pdu& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "PDU");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    appendTextChild(node, "LENGTH", std::to_string(value.length));
    // TODO(PAR-146): Pdu signal mappings (PDU-SIGNAL-MAPPING / SIGNAL-REF /
    // START-POSITION / BYTE-ORDER) — nested-collection structure goes here.
    return node;
}

xmlNodePtr buildSignalElement(xmlDocPtr doc, const Signal& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "SYSTEM-SIGNAL");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    appendTextChild(node, "START-BIT", std::to_string(value.startBit));
    appendTextChild(node, "BIT-LENGTH", std::to_string(value.bitLength));
    appendTextChild(node, "BYTE-ORDER", byteOrderToString(value.byteOrder));
    // TODO(PAR-146): value-table entries (VALUE-TABLE / VALUE-TABLE-ENTRY /
    // VALUE / LABEL) — nested-collection structure goes here.
    if (!value.receivers.empty()) {
        xmlNodePtr receivers = appendChild(node, "RECEIVERS");
        for (const std::string& ecu : value.receivers) {
            xmlNodePtr ref = appendChild(receivers, "RECEIVER-REF");
            xmlNewProp(ref, BAD_CAST "DEST", BAD_CAST "ECU-INSTANCE");
            setNodeText(ref, "/Sys/" + ecu);
        }
    }
    return node;
}

xmlNodePtr buildSignalGroupElement(xmlDocPtr doc, const SignalGroup& value) {
    (void)doc;
    xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "SIGNAL-GROUP");
    appendTextChild(node, "SHORT-NAME", value.common.shortName);
    // TODO(PAR-146): SignalGroup member-signal list (MEMBERS /
    // SYSTEM-SIGNAL-REF) — nested-collection structure goes here.
    return node;
}
