#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <parsex/diff/diff_fields.hpp>

// Key-based nested-collection diffing (PAR-128).
//
// Collections nested inside a matched element (SignalGroup members, Frame PDU
// mappings, Pdu signal mappings, value-table entries) are matched by identity
// key, not list position — the same key-over-position principle as PAR-116
// (and protobuf's TreatAsMap/MapKeyComparator). Reordering alone never
// reports as a change.

// Generic key-based collection diff. keyOf maps an item to its identity key.
// itemDiff, when provided, diffs two matched items with the same key (defaults
// to diffStruct for aggregates, empty for strings).
template <typename T, typename KeyFn, typename ItemDiffFn>
[[nodiscard]] std::vector<FieldDiff> diffNestedCollectionWith(
    const std::vector<T>& oldItems, const std::vector<T>& newItems, KeyFn keyOf,
    const std::string& fieldName, ItemDiffFn itemDiff) {
    std::map<std::string, const T*> oldByKey;
    for (const auto& item : oldItems) {
        oldByKey.try_emplace(keyOf(item), &item);
    }
    std::map<std::string, const T*> newByKey;
    for (const auto& item : newItems) {
        newByKey.try_emplace(keyOf(item), &item);
    }

    std::vector<FieldDiff> diffs;
    for (const auto& [key, oldPtr] : oldByKey) {
        if (!newByKey.contains(key)) {
            diffs.push_back({.fieldName = fieldName + "[" + key + "]",
                             .oldValue = diffValueToString(*oldPtr),
                             .newValue = "absent"});
        }
    }
    for (const auto& [key, newPtr] : newByKey) {
        if (!oldByKey.contains(key)) {
            diffs.push_back({.fieldName = fieldName + "[" + key + "]",
                             .oldValue = "absent",
                             .newValue = diffValueToString(*newPtr)});
        }
    }
    for (const auto& [key, oldPtr] : oldByKey) {
        const auto it = newByKey.find(key);
        if (it == newByKey.end()) {
            continue;
        }
        for (FieldDiff sub : itemDiff(*oldPtr, *it->second)) {
            diffs.push_back({.fieldName = fieldName + "[" + key + "]." + sub.fieldName,
                             .oldValue = sub.oldValue,
                             .newValue = sub.newValue});
        }
    }
    return diffs;
}

template <typename T, typename KeyFn>
[[nodiscard]] std::vector<FieldDiff> diffNestedCollection(const std::vector<T>& oldItems,
                                                          const std::vector<T>& newItems,
                                                          KeyFn keyOf,
                                                          const std::string& fieldName) {
    if constexpr (std::is_same_v<T, std::string>) {
        return diffNestedCollectionWith(oldItems, newItems, keyOf, fieldName,
                                        [](const T&, const T&) { return std::vector<FieldDiff>{}; });
    } else {
        return diffNestedCollectionWith(
            oldItems, newItems, keyOf, fieldName,
            [](const T& o, const T& n) { return diffStruct(o, n); });
    }
}

// --- per-collection key extractors (from the Common Domain Model) ---

[[nodiscard]] inline std::string signalGroupMemberKey(const std::string& member) { return member; }

[[nodiscard]] inline std::string framePduMappingKey(const FramePduMapping& m) {
    return m.pduShortNameRef;
}

[[nodiscard]] inline std::string pduSignalMappingKey(const PduSignalMapping& m) {
    return m.signalShortNameRef;
}

[[nodiscard]] inline std::string valueTableEntryKey(const ValueTableEntry& e) {
    return std::to_string(e.value);
}

// --- element-level helpers combining flat + nested diffs ---
//
// diffStruct() treats vector members opaquely; these helpers replace the
// opaque collection FieldDiffs with key-based nested diffs so reordering
// alone yields zero diffs and additions/removals are keyed.

[[nodiscard]] inline std::vector<FieldDiff> diffSignalGroupElements(const SignalGroup& oldG,
                                                                    const SignalGroup& newG) {
    std::vector<FieldDiff> diffs = diffStruct(oldG, newG);
    diffs.erase(std::remove_if(diffs.begin(), diffs.end(),
                               [](const FieldDiff& d) { return d.fieldName == "members"; }),
                diffs.end());
    for (FieldDiff nested : diffNestedCollection(oldG.members, newG.members,
                                                 signalGroupMemberKey, "members")) {
        diffs.push_back(std::move(nested));
    }
    return diffs;
}

[[nodiscard]] inline std::vector<FieldDiff> diffFrameElements(const Frame& oldF, const Frame& newF) {
    std::vector<FieldDiff> diffs = diffStruct(oldF, newF);
    diffs.erase(std::remove_if(diffs.begin(), diffs.end(),
                               [](const FieldDiff& d) { return d.fieldName == "pdus"; }),
                diffs.end());
    for (FieldDiff nested :
         diffNestedCollection(oldF.pdus, newF.pdus, framePduMappingKey, "pdus")) {
        diffs.push_back(std::move(nested));
    }
    // transmitters is a plain string list with no stable sub-identity beyond
    // the value itself; key-based (value-keyed) diff still beats positional:
    // a reordered transmitter list yields zero diffs.
    std::vector<FieldDiff> txOpaque;
    for (const FieldDiff& d : diffStruct(oldF, newF)) {
        if (d.fieldName == "transmitters") {
            txOpaque.push_back(d);
        }
    }
    if (!txOpaque.empty()) {
        diffs.erase(std::remove_if(diffs.begin(), diffs.end(),
                                   [](const FieldDiff& d) { return d.fieldName == "transmitters"; }),
                    diffs.end());
        for (FieldDiff nested : diffNestedCollection(oldF.transmitters, newF.transmitters,
                                                     signalGroupMemberKey, "transmitters")) {
            diffs.push_back(std::move(nested));
        }
    }
    return diffs;
}

[[nodiscard]] inline std::vector<FieldDiff> diffPduElements(const Pdu& oldP, const Pdu& newP) {
    std::vector<FieldDiff> diffs = diffStruct(oldP, newP);
    diffs.erase(std::remove_if(diffs.begin(), diffs.end(),
                               [](const FieldDiff& d) { return d.fieldName == "signalMappings"; }),
                diffs.end());
    for (FieldDiff nested : diffNestedCollection(oldP.signalMappings, newP.signalMappings,
                                                 pduSignalMappingKey, "signalMappings")) {
        diffs.push_back(std::move(nested));
    }
    return diffs;
}

[[nodiscard]] inline std::vector<FieldDiff> diffSignalValueTable(
    const std::optional<std::vector<ValueTableEntry>>& oldTable,
    const std::optional<std::vector<ValueTableEntry>>& newTable) {
    const std::vector<ValueTableEntry> oldItems = oldTable.value_or(std::vector<ValueTableEntry>{});
    const std::vector<ValueTableEntry> newItems = newTable.value_or(std::vector<ValueTableEntry>{});
    if (oldItems.empty() && newItems.empty()) {
        // Both absent, or both present-but-empty: no diff. (Absent vs empty
        // is not a semantic change worth reporting.)
        if (oldTable.has_value() == newTable.has_value()) {
            return {};
        }
    }
    return diffNestedCollection(oldItems, newItems, valueTableEntryKey, "valueTable");
}

[[nodiscard]] inline std::vector<FieldDiff> diffSignalElements(const Signal& oldS, const Signal& newS) {
    std::vector<FieldDiff> diffs = diffStruct(oldS, newS);
    // Replace opaque valueTable + receivers diffs with key-based ones.
    diffs.erase(std::remove_if(diffs.begin(), diffs.end(),
                               [](const FieldDiff& d) {
                                   return d.fieldName == "valueTable" || d.fieldName == "receivers";
                               }),
                diffs.end());
    for (FieldDiff nested : diffSignalValueTable(oldS.valueTable, newS.valueTable)) {
        diffs.push_back(std::move(nested));
    }
    for (FieldDiff nested : diffNestedCollection(oldS.receivers, newS.receivers,
                                                 signalGroupMemberKey, "receivers")) {
        diffs.push_back(std::move(nested));
    }
    return diffs;
}
