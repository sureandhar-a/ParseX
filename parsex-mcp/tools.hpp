#pragma once

#include <filesystem>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include <parsex/diff/diff_engine.hpp>
#include <parsex/json_contract/envelope.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/schema/schema_registry.hpp>
#include <parsex/validator/validator.hpp>
#include <parsex/write/write_engine.hpp>

// Engine wiring shared by the local bridge and the command-line surface.
// Handlers stay thin: extract arguments, call the engine, pack the result.
// Exceptions propagate to the shared caller, which turns them into tool errors.

// --- Parse ---

inline std::string humanParseSummary(const ParsedFile& file) {
    std::ostringstream out;
    out << "Parsed " << file.sourcePath.string() << "\n";
    out << "  release: " << (file.autosarRelease.empty() ? "(unknown)" : file.autosarRelease)
        << "\n";
    out << "  clusters: " << file.clusters.size() << "\n";
    out << "  ecuInstances: " << file.ecuInstances.size() << "\n";
    out << "  frames: " << file.frames.size() << "\n";
    out << "  pdus: " << file.pdus.size() << "\n";
    out << "  signals: " << file.signals.size() << "\n";
    out << "  signalGroups: " << file.signalGroups.size() << "\n";
    return out.str();
}

inline nlohmann::json parseEnvelope(const ParsedFile& file) {
    nlohmann::json payload;
    payload["sourcePath"] = file.sourcePath.string();
    payload["autosarRelease"] = file.autosarRelease;
    payload["counts"] = {{"clusters", file.clusters.size()},
                         {"ecuInstances", file.ecuInstances.size()},
                         {"frames", file.frames.size()},
                         {"pdus", file.pdus.size()},
                         {"signals", file.signals.size()},
                         {"signalGroups", file.signalGroups.size()}};
    nlohmann::json warnings = nlohmann::json::array();
    for (const auto& warning : file.warnings) {
        warnings.push_back({{"message", warning.message}});
    }
    payload["warnings"] = std::move(warnings);
    return parsex::json_contract::wrapEnvelope("parseReport", std::move(payload));
}

inline nlohmann::json parseTool(const nlohmann::json& args) {
    const std::string path = args.at("path").get<std::string>();
    Parser parser;
    ParsedFile file = parser.parseFile(std::filesystem::path(path));
    nlohmann::json packed;
    packed["structuredContent"] = parseEnvelope(file);
    packed["summary"] = humanParseSummary(file);
    return packed;
}
