#pragma once

#include <libxml/tree.h>

#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>

// Per-domain-type element builders (PAR-145/146, post-pivot): one function
// per domain type emitting REAL AUTOSAR 4.x vocabulary (CAN-CLUSTER,
// ECU-INSTANCE, CAN-FRAME, I-SIGNAL-I-PDU, I-SIGNAL, I-SIGNAL-GROUP —
// verified against resources/schemas/4.4.0.xsd), using the same element names
// the Parser/Loader already reads, so read and write stay in sync and output
// is genuinely schema-valid ARXML.
//
// Each builder creates a detached element (caller attaches via xmlAddChild).
// See write_elements.cpp for the v1 scope limits (fields with no
// single-node schema-valid home).
xmlNodePtr buildClusterElement(xmlDocPtr doc, const Cluster& value);
xmlNodePtr buildEcuInstanceElement(xmlDocPtr doc, const EcuInstance& value);
xmlNodePtr buildFrameElement(xmlDocPtr doc, const Frame& value);
xmlNodePtr buildPduElement(xmlDocPtr doc, const Pdu& value);
xmlNodePtr buildSignalElement(xmlDocPtr doc, const Signal& value);
xmlNodePtr buildSignalGroupElement(xmlDocPtr doc, const SignalGroup& value);

// Shared helpers (also used by PAR-146).
xmlNodePtr appendChild(xmlNodePtr parent, const char* tagName);
void setNodeText(xmlNodePtr node, const std::string& text);
void appendTextChild(xmlNodePtr parent, const char* tagName, const std::string& text);
void appendOptionalTextChild(xmlNodePtr parent, const char* tagName,
                             const std::optional<std::string>& value);
std::string byteOrderToString(ByteOrder order);
