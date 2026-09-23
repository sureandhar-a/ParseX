#pragma once

#include <libxml/tree.h>

#include <algorithm>
#include <string>
#include <vector>

#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>

// Deterministic ordering passes (PAR-148/149), per AUTOSAR ARXML
// Serialization Rules R20-11.
//
// TPS_ASR_00019: XML attributes sorted alphabetically. Applied via
// setSortedAttributes() — every per-type builder routes attribute-setting
// through this helper instead of calling xmlNewProp directly in ad hoc order.
//
// Exempt (by design, not oversight): the root <AUTOSAR> element's namespace
// declarations and xsi:schemaLocation keep their conventional position —
// real ARXML samples show them first/in fixed position, so they are never
// alphabetized with ordinary domain-type attributes.
void setSortedAttributes(
    xmlNodePtr element,
    const std::vector<std::pair<std::string, std::string>>& attributes);

// TPS_ASR_00014: semantically-unordered repeated-element collections are
// sorted by short-name before emitting (PAR-149). Classification per type is
// documented on each sort helper — future contributors need the reasoning
// since it is not mechanically derivable.
template <typename T, typename KeyFn>
void sortByKey(std::vector<T>& items, KeyFn key) {
    std::ranges::sort(items, {}, key);
}

// Classification (per Common Domain-Model field semantics):
// - UNORDERED (sorted by short-name/stable key): Cluster.physicalChannels,
//   EcuInstance.connectedChannels/controllers, Frame.transmitters,
//   Frame.pdus (by pduShortNameRef — START-POSITION is an attribute, list
//   order itself carries no meaning), Pdu.signalMappings (by
//   signalShortNameRef), Signal.receivers, SignalGroup.members,
//   Signal.valueTable (by numeric code), and every top-level domain vector
//   (clusters, ecuInstances, frames, pdus, signals, signalGroups by
//   common.shortName).
// - ORDERED (never resorted, preserved exactly): no v1 domain
//   repeated-element collection. The ordered branch exists for future AUTOSAR
//   sequences with meaningful position (preserveElementOrder documents and
//   tests it) — today every repeated structure above is unordered.
std::vector<std::string> sortedStrings(std::vector<std::string> names);
std::vector<FramePduMapping> sortedFramePduMappings(std::vector<FramePduMapping> mappings);
std::vector<PduSignalMapping> sortedPduSignalMappings(
    std::vector<PduSignalMapping> mappings);
std::vector<ValueTableEntry> sortedValueTableEntries(std::vector<ValueTableEntry> entries);

// Ordered-branch helper: returns the input unchanged. Used (today only in
// tests) to prove the classification actually branches instead of sorting
// everything unconditionally.
template <typename T>
std::vector<T> preserveElementOrder(std::vector<T> items) {
    return items;
}
