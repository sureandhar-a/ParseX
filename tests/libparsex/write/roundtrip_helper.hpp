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
    // parseFile (not parseProject): round-trip equality is per-file, and
    // real-world fixtures (e.g. system-4.2.arxml) reference PDU types outside
    // the six built families, which parseProject's cross-file gate rejects.
    ParsedProject original;
    original.files.push_back(Parser{}.parseFile(originalArxmlPath));
    const std::filesystem::path temp =
        std::filesystem::temp_directory_path() / "parsex_roundtrip_tmp.arxml";
    WriteEngine{}.write(original, temp);
    ParsedProject reparsed;
    reparsed.files.push_back(Parser{}.parseFile(temp));
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
