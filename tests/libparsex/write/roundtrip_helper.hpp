#pragma once

// Round-trip test helper (PAR-154): parse -> write -> reparse -> diff.
// A correct round trip returns an empty DiffReport. Failures include the
// rendered toText() so a failing test shows exactly which fields differ.

#include <parsex/diff/diff_engine.hpp>
#include <parsex/diff/diff_report.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/write/write_engine.hpp>

#include <filesystem>
#include <string>

inline DiffReport roundTripDiff(const std::filesystem::path& originalArxmlPath) {
    const ParsedProject original =
        Parser{}.parseProject({originalArxmlPath}, FileDiscoveryMode::ExplicitList);
    const std::filesystem::path temp =
        std::filesystem::temp_directory_path() / "parsex_roundtrip_tmp.arxml";
    WriteEngine{}.write(original, temp);
    const ParsedProject reparsed =
        Parser{}.parseProject({temp}, FileDiscoveryMode::ExplicitList);
    std::error_code dropError;
    std::filesystem::remove(temp, dropError);
    return DiffEngine{}.diff(original, reparsed);
}

inline void assertRoundTripsCleanly(const std::filesystem::path& path) {
    const DiffReport report = roundTripDiff(path);
    if (!report.empty()) {
        FAIL() << "round-trip diff for '" << path.string() << "' is not empty:\n"
               << report.toText();
    }
}
