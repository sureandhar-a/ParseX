#pragma once

#include <libxml/tree.h>

#include <algorithm>
#include <string>
#include <vector>

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
