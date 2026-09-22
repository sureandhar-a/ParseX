#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>

#include <parsex/diff/diff_report.hpp>
#include <parsex/model/parsed_project.hpp>

// Secondary-key extraction for move detection (PAR-124).
//
// No stable key survives a path change, so — modeled on git's
// similarity-based rename detection — this uses domain-specific identity-like
// fields instead of a generic similarity score.
//
// NOTE on field availability: the task description expects a numeric CAN ID
// on Frame and a parent-Pdu identifier on Signal, but the current Common
// Domain Model carries neither (Frame has length/transmitters/pdu mappings;
// Signal has startBit/bitLength/byteOrder/receivers). The keys below use the
// most identity-like non-path fields actually present; see TODO in
// diff_moves.cpp for the CAN-ID gap.
[[nodiscard]] std::optional<std::string> secondaryKey(const Cluster& c);
[[nodiscard]] std::optional<std::string> secondaryKey(const EcuInstance& e);
[[nodiscard]] std::optional<std::string> secondaryKey(const Frame& f);
[[nodiscard]] std::optional<std::string> secondaryKey(const Pdu& p);
[[nodiscard]] std::optional<std::string> secondaryKey(const Signal& s);
[[nodiscard]] std::optional<std::string> secondaryKey(const SignalGroup& g);

// Correlate Removed/Added entries of one domain type into Moved entries.
//
// Only touches entries with elementType == elementTypeName. Groups Removed
// and Added entries by secondary key (looked up via oldIndex/newIndex); a
// group with exactly one Removed and one Added becomes a single Moved entry.
// Ambiguous groups (both sides present but not a clean 1:1 pairing) are left
// as Removed/Added and gain a DiffDiagnostic (PAR-125) — never silently
// guessed. Groups with only one side present are simply unmatched, no
// diagnostic.
template <typename T>
void detectMovesForType(const std::map<std::string, const T*>& oldIndex,
                        const std::map<std::string, const T*>& newIndex, DiffReport& report,
                        const std::string& elementTypeName) {
    struct Group {
        std::vector<std::size_t> removedIdx;
        std::vector<std::size_t> addedIdx;
    };
    std::map<std::string, Group> groups;
    for (std::size_t i = 0; i < report.entries.size(); ++i) {
        const DiffEntry& entry = report.entries[i];
        if (entry.elementType != elementTypeName) {
            continue;
        }
        if (entry.kind == DiffKind::Removed) {
            const auto it = oldIndex.find(entry.oldPath);
            if (it == oldIndex.end() || it->second == nullptr) {
                continue;
            }
            if (const auto key = secondaryKey(*it->second); key.has_value()) {
                groups[*key].removedIdx.push_back(i);
            }
        } else if (entry.kind == DiffKind::Added) {
            const auto it = newIndex.find(entry.newPath);
            if (it == newIndex.end() || it->second == nullptr) {
                continue;
            }
            if (const auto key = secondaryKey(*it->second); key.has_value()) {
                groups[*key].addedIdx.push_back(i);
            }
        }
    }

    std::vector<std::size_t> addedToErase;
    for (auto& [key, group] : groups) {
        if (group.removedIdx.size() == 1 && group.addedIdx.size() == 1) {
            DiffEntry& removed = report.entries[group.removedIdx[0]];
            const DiffEntry& added = report.entries[group.addedIdx[0]];
            removed.kind = DiffKind::Moved;
            removed.newPath = added.newPath;
            addedToErase.push_back(group.addedIdx[0]);
        } else if (!group.removedIdx.empty() && !group.addedIdx.empty()) {
            const std::size_t total = group.removedIdx.size() + group.addedIdx.size();
            std::string message = std::to_string(total) + " candidates shared secondary key " +
                                  key + " for type " + elementTypeName + ", no move inferred (";
            bool first = true;
            for (const std::size_t idx : group.removedIdx) {
                if (!first) {
                    message += ", ";
                }
                first = false;
                message += "removed:" + report.entries[idx].oldPath;
            }
            for (const std::size_t idx : group.addedIdx) {
                if (!first) {
                    message += ", ";
                }
                first = false;
                message += "added:" + report.entries[idx].newPath;
            }
            message += ")";
            report.diagnostics.push_back({.message = std::move(message),
                                           .elementType = elementTypeName});
        }
    }
    if (!addedToErase.empty()) {
        std::vector<DiffEntry> kept;
        kept.reserve(report.entries.size() - addedToErase.size());
        std::size_t erasePos = 0;
        std::sort(addedToErase.begin(), addedToErase.end());
        for (std::size_t i = 0; i < report.entries.size(); ++i) {
            if (erasePos < addedToErase.size() && addedToErase[erasePos] == i) {
                ++erasePos;
                continue;
            }
            kept.push_back(std::move(report.entries[i]));
        }
        report.entries = std::move(kept);
    }
}

// Whole-report move detection across all six domain types. Builds indexes
// from the two projects, then runs detectMovesForType per type.
[[nodiscard]] DiffReport applyMoveDetection(const ParsedProject& oldProject,
                                            const ParsedProject& newProject, DiffReport matched);
