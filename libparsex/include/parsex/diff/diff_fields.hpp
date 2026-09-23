#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <boost/pfr.hpp>

#include <parsex/diff/diff_report.hpp>
#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>

// Generic field-level struct comparison (PAR-127).
//
// Uses Boost.PFR (macro-free aggregate reflection) to iterate fields without
// per-type boilerplate. All six domain types plus their nested structs are
// plain aggregates (verified by static_assert below), so PFR applies; the
// legitimate fallback noted in the task (hand-written tables for
// non-aggregates) is not needed today — protobuf's generated classes need it,
// ParseX's do not.
//
// Field names come from small parallel constexpr arrays kept next to the
// structs (fallback path in the task); tuple_size static_asserts keep them
// from silently drifting when a field is added.

// --- value stringification ------------------------------------------------

[[nodiscard]] inline std::string diffValueToString(const std::string& v) { return v; }
[[nodiscard]] inline std::string diffValueToString(std::string_view v) { return std::string(v); }
[[nodiscard]] inline std::string diffValueToString(const char* v) { return v != nullptr ? v : ""; }
[[nodiscard]] inline std::string diffValueToString(bool v) { return v ? "true" : "false"; }

template <typename T>
    requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
[[nodiscard]] std::string diffValueToString(const T& v) {
    if constexpr (std::is_floating_point_v<T>) {
        std::ostringstream oss;
        oss.precision(17);
        oss << v;
        return oss.str();
    } else {
        return std::to_string(v);
    }
}

[[nodiscard]] inline std::string diffValueToString(ByteOrder o) {
    return o == ByteOrder::MostSignificantByteFirst ? "MostSignificantByteFirst"
                                                    : "LeastSignificantByteFirst";
}

// Source locations are intentionally excluded from semantic diffing: two
// identical elements parsed from different files must not report as changed.
[[nodiscard]] inline std::string diffValueToString(const RawSpan& /*span*/) { return "span"; }

[[nodiscard]] inline std::string diffValueToString(const CommonFields& c) {
    // Identity-excluded: shortName is the element's identity (matched by path
    // in PAR-122, correlated by secondary key in PAR-124), not a field
    // change. A Moved entry's old/new shortNames differ by definition — that
    // is the move itself, reported via oldPath/newPath, and must not also
    // appear as a spurious "common" FieldDiff (PAR-135 needs plain moves to
    // carry empty fieldDiffs).
    return std::string("category=") + (c.category.has_value() ? *c.category : "nullopt");
}

[[nodiscard]] inline std::string diffValueToString(const FramePduMapping& m) {
    return m.pduShortNameRef + "@" + std::to_string(m.startPosition);
}

[[nodiscard]] inline std::string diffValueToString(const PduSignalMapping& m) {
    std::string order =
        m.byteOrder == ByteOrder::MostSignificantByteFirst ? "M" : "L";
    return m.signalShortNameRef + "@" + std::to_string(m.startPosition) + order;
}

[[nodiscard]] inline std::string diffValueToString(const ValueTableEntry& e) {
    return std::to_string(e.value) + ":" + e.label;
}

template <typename T>
[[nodiscard]] std::string diffValueToString(const std::optional<T>& v) {
    if (!v.has_value()) {
        return "nullopt";
    }
    return diffValueToString(*v);
}

template <typename T>
[[nodiscard]] std::string diffValueToString(const std::vector<T>& v) {
    std::string out = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }
        out += diffValueToString(v[i]);
    }
    out += "]";
    return out;
}

// --- field-name tables -----------------------------------------------------

template <typename T>
constexpr auto diffFieldNames();

template <>
constexpr auto diffFieldNames<CommonFields>() {
    return std::array<std::string_view, 3>{"shortName", "category", "rawSpanRef"};
}
template <>
constexpr auto diffFieldNames<Cluster>() {
    return std::array<std::string_view, 3>{"common", "baudrate", "physicalChannels"};
}
template <>
constexpr auto diffFieldNames<EcuInstance>() {
    return std::array<std::string_view, 3>{"common", "connectedChannels", "controllers"};
}
template <>
constexpr auto diffFieldNames<FramePduMapping>() {
    return std::array<std::string_view, 2>{"pduShortNameRef", "startPosition"};
}
template <>
constexpr auto diffFieldNames<Frame>() {
    return std::array<std::string_view, 4>{"common", "length", "transmitters", "pdus"};
}
template <>
constexpr auto diffFieldNames<PduSignalMapping>() {
    return std::array<std::string_view, 3>{"signalShortNameRef", "startPosition", "byteOrder"};
}
template <>
constexpr auto diffFieldNames<Pdu>() {
    return std::array<std::string_view, 3>{"common", "length", "signalMappings"};
}
template <>
constexpr auto diffFieldNames<ValueTableEntry>() {
    return std::array<std::string_view, 2>{"value", "label"};
}
template <>
constexpr auto diffFieldNames<Signal>() {
    return std::array<std::string_view, 13>{"common",     "startBit", "bitLength", "byteOrder",
                                            "isSigned",   "factor",   "offset",    "min",
                                            "max",        "unit",     "initValue", "receivers",
                                            "valueTable"};
}
template <>
constexpr auto diffFieldNames<SignalGroup>() {
    return std::array<std::string_view, 2>{"common", "members"};
}

static_assert(boost::pfr::tuple_size_v<CommonFields> == 3);
static_assert(boost::pfr::tuple_size_v<Cluster> == 3);
static_assert(boost::pfr::tuple_size_v<EcuInstance> == 3);
static_assert(boost::pfr::tuple_size_v<Frame> == 4);
static_assert(boost::pfr::tuple_size_v<Pdu> == 3);
static_assert(boost::pfr::tuple_size_v<Signal> == 13);
static_assert(boost::pfr::tuple_size_v<SignalGroup> == 2);
static_assert(boost::pfr::tuple_size_v<FramePduMapping> == 2);
static_assert(boost::pfr::tuple_size_v<PduSignalMapping> == 3);
static_assert(boost::pfr::tuple_size_v<ValueTableEntry> == 2);

// --- generic comparator ----------------------------------------------------

template <typename T, std::size_t... I>
void diffStructImpl(const T& oldVal, const T& newVal, std::vector<FieldDiff>& out,
                    std::index_sequence<I...>) {
    constexpr auto names = diffFieldNames<T>();
    static_assert(names.size() == boost::pfr::tuple_size_v<T>);
    (([&] {
        const auto& oldField = boost::pfr::get<I>(oldVal);
        const auto& newField = boost::pfr::get<I>(newVal);
        const std::string oldStr = diffValueToString(oldField);
        const std::string newStr = diffValueToString(newField);
        if (oldStr != newStr) {
            out.push_back({.fieldName = std::string(names[I]),
                           .oldValue = oldStr,
                           .newValue = newStr});
        }
    }()),
     ...);
}

template <typename T>
[[nodiscard]] std::vector<FieldDiff> diffStruct(const T& oldVal, const T& newVal) {
    std::vector<FieldDiff> diffs;
    diffStructImpl(oldVal, newVal, diffs,
                   std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
    return diffs;
}
