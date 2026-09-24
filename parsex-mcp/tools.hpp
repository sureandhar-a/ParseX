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

// --- Validate ---

inline std::string humanValidateSummary(const ValidationResult& result, bool strict) {
    const bool passed =
        Validator::overallPassed(result, strict ? StrictMode::Strict : StrictMode::Lenient);
    std::ostringstream out;
    out << (passed ? "Validation passed" : "Validation failed") << " (" << result.errors.size()
        << " issues)\n";
    for (const auto& error : result.errors) {
        out << "  [" << (error.severity == Severity::Warning ? "warning" : "error") << "] "
            << error.code << ": " << error.message << "\n";
        if (error.path) {
            out << "    path: " << *error.path << "\n";
        }
    }
    return out.str();
}

inline nlohmann::json validateEnvelope(const ValidationResult& result, bool strict) {
    nlohmann::json envelope = result.toJson();
    const bool passed =
        Validator::overallPassed(result, strict ? StrictMode::Strict : StrictMode::Lenient);
    envelope["payload"]["passed"] = passed;
    return envelope;
}

inline nlohmann::json validateTool(const nlohmann::json& args) {
    const std::string path = args.at("path").get<std::string>();
    const bool strict = args.value("strict", false);
    Parser parser;
    ParsedProject project;
    ValidationResult result;
    bool parsedOk = false;
    try {
        ParsedFile file = parser.parseFile(std::filesystem::path(path));
        project.files.push_back(std::move(file));
        parsedOk = true;
    } catch (const std::exception& ex) {
        result.errors.push_back({Severity::Error, "parse.failed", ex.what()});
        parsedOk = false;
    }
    if (parsedOk) {
        try {
            const std::string& release = project.files.front().autosarRelease;
            if (!release.empty()) {
                auto resolved = resolveSchema(release);
                Validator validator;
                ValidationResult combined =
                    validator.validateAll(project, resolved.schema.schemaHandle.get());
                result.merge(combined);
            } else {
                result.errors.push_back(
                    {Severity::Error,
                     "schema.no_release",
                     "cannot determine AUTOSAR release for validation"});
            }
        } catch (const std::exception& ex) {
            result.errors.push_back({Severity::Error, "validator.error", ex.what()});
        }
    }
    nlohmann::json packed;
    packed["structuredContent"] = validateEnvelope(result, strict);
    packed["summary"] = humanValidateSummary(result, strict);
    return packed;
}

// --- Diff ---

inline std::string humanDiffSummary(const DiffReport& report) {
    if (report.empty()) {
        return "No differences\n";
    }
    return report.toText();
}

inline nlohmann::json diffTool(const nlohmann::json& args) {
    const std::string base = args.at("basePath").get<std::string>();
    const std::string target = args.at("targetPath").get<std::string>();
    Parser parser;
    ParsedProject oldProject;
    ParsedProject newProject;
    oldProject.files.push_back(parser.parseFile(std::filesystem::path(base)));
    newProject.files.push_back(parser.parseFile(std::filesystem::path(target)));
    DiffEngine engine;
    DiffReport report = engine.diff(oldProject, newProject);
    nlohmann::json packed;
    packed["structuredContent"] = report.toJson();
    packed["summary"] = humanDiffSummary(report);
    return packed;
}
