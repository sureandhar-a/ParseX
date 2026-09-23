#pragma once

#include <libxml/tree.h>

#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>

// Per-domain-type element builders (PAR-145): one function per domain type,
// mapping fields to the same AUTOSAR element names the Parser/Loader already
// reads (study vocabulary — see model_builder.hpp and
// tests/fixtures/parsefile_complete.arxml), so read and write stay in sync.
//
// Each builder creates a detached element (caller attaches via xmlAddChild).
// Nested-collection fields (SignalGroup members, Frame PDU mappings, Pdu
// signal mappings, value tables) are TODO for PAR-146 — see markers below.
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
