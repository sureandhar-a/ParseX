#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include "CLI/CLI.hpp"
#include <nlohmann/json.hpp>
#include "parsex/diff/diff_engine.hpp"
#include "parsex/diff/diff_report.hpp"
#include "parsex/json_contract/envelope.hpp"
#include "parsex/parser/parser.hpp"
#include "parsex/schema/cache_layout.hpp"
#include "parsex/schema/schema_registry.hpp"
#include "parsex/telemetry/scoped_span.hpp"
#include "parsex/telemetry/telemetry_config.hpp"
#include "parsex/telemetry/telemetry_context.hpp"
#include "parsex/validator/validator.hpp"
#include "parsex/validator/validation_result.hpp"
#include "parsex/write/write_engine.hpp"
#include "parsex/version.hpp"
#include "color.hpp"
#include "exit_code.hpp"

using namespace std;

namespace {

// PAR-221: precedence helper flag > env > config > default.
// Returns effective cache dir and source name; also applies the override via
// setCacheDirectoryOverride() when not default. Logs at debug level to stderr
// (not stdout) so --json's stdout stays clean. argc/argv are needed to detect
// an explicit --schema-cache-dir flag, because CLI11's config-file values are
// indistinguishable via count().
inline std::pair<std::filesystem::path, std::string> resolveSchemaCacheDir(
    int argc, char const* const* argv, const std::string& boundValue) {
    bool flagPresent = false;
    for (int idx = 1; idx < argc; ++idx) {
        // argv indexing is mandated by the OS main() contract; bounds enforced by argc.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::string arg = argv[idx];
        if (arg == "--schema-cache-dir" || arg.starts_with("--schema-cache-dir=")) {
            flagPresent = true;
            break;
        }
    }
    if (flagPresent) {
        return {std::filesystem::path(boundValue), "flag"};
    }
    if (const char* env = std::getenv("PARSEX_SCHEMA_CACHE_DIR");
        env != nullptr && *env != '\0') {
        return {std::filesystem::path(env), "env"};
    }
    if (!boundValue.empty()) {
        return {std::filesystem::path(boundValue), "config"};
    }
    return {{}, "default"};
}

inline void applySchemaCacheDir(int argc, char const* const* argv, const std::string& boundValue) {
    static bool applied = false;
    if (applied) {
        return;
    }
    applied = true;
    auto [effective, source] = resolveSchemaCacheDir(argc, argv, boundValue);
    if (!effective.empty()) {
        setCacheDirectoryOverride(effective);
        std::cerr << "[debug] schema-cache-dir=" << effective.string() << " (" << source << ")\n";
    } else {
        std::cerr << "[debug] schema-cache-dir=" << getCacheDirectory().string() << " (" << source << ")\n";
    }
}

// PAR-224: human-readable formatting for Parser output (real engine, not placeholder).
inline std::string formatHumanParse(const ParsedFile& parsed) {
    std::ostringstream oss;
    oss << "Parsed " << parsed.sourcePath.string() << "\n";
    oss << "  release: " << (parsed.autosarRelease.empty() ? "(unknown)" : parsed.autosarRelease) << "\n";
    oss << "  clusters: " << parsed.clusters.size() << "\n";
    oss << "  ecuInstances: " << parsed.ecuInstances.size() << "\n";
    oss << "  frames: " << parsed.frames.size() << "\n";
    oss << "  pdus: " << parsed.pdus.size() << "\n";
    oss << "  signals: " << parsed.signals.size() << "\n";
    oss << "  signalGroups: " << parsed.signalGroups.size() << "\n";
    if (!parsed.warnings.empty()) {
        oss << "  warnings: " << parsed.warnings.size() << "\n";
        for (const auto& warning : parsed.warnings) {
            oss << "    - " << warning.message << "\n";
        }
    }
    return oss.str();
}

inline std::string formatHumanValidate(const std::string& input) {
    return "Validated " + input + ": OK (placeholder)";
}
inline std::string formatHumanValidate(const ValidationResult& result, bool strict) {
    bool passed = Validator::overallPassed(result, strict ? StrictMode::Strict : StrictMode::Lenient);
    std::ostringstream oss;
    oss << (passed ? "Validation passed" : "Validation failed") << " (" << result.errors.size() << " issues)\n";
    for (const auto& error : result.errors) {
        oss << "  [" << (error.severity == Severity::Warning ? "warning" : "error") << "] " << error.code << ": " << error.message << "\n";
        if (error.path) {
            oss << "    path: " << *error.path << "\n";
        }
    }
    return oss.str();
}
inline std::string formatHumanDiff(const std::string& base, const std::string& target) {
    return "Diff " + base + " vs " + target + ": no differences (placeholder)";
}
inline std::string formatHumanDiff(const DiffReport& report) {
    // Use the engine's own deterministic text rendering (grouped by category).
    if (report.empty()) {
        return "No differences\n";
    }
    return report.toText();
}
inline std::string formatHumanWrite(const std::string& input) {
    return "Wrote " + input + " (placeholder, dry-run)";
}
inline std::string formatHumanWrite(const std::filesystem::path& input, const std::filesystem::path& output, bool applied) {
    std::ostringstream oss;
    if (applied) {
        oss << "Wrote " << output.string() << " from " << input.string() << "\n";
    } else {
        oss << "Dry-run: would write " << output.string() << " from " << input.string() << " (use --apply to apply)\n";
    }
    return oss.str();
}

// PAR-224: JSON via shared envelope — parseReport payload with counts.
inline nlohmann::json buildJsonParse(const ParsedFile& parsed) {
    nlohmann::json payload;
    payload["sourcePath"] = parsed.sourcePath.string();
    payload["autosarRelease"] = parsed.autosarRelease;
    payload["counts"] = {{"clusters", parsed.clusters.size()},
                         {"ecuInstances", parsed.ecuInstances.size()},
                         {"frames", parsed.frames.size()},
                         {"pdus", parsed.pdus.size()},
                         {"signals", parsed.signals.size()},
                         {"signalGroups", parsed.signalGroups.size()}};
    nlohmann::json warnings = nlohmann::json::array();
    for (const auto& warning : parsed.warnings) {
        warnings.push_back({{"message", warning.message}});
    }
    payload["warnings"] = std::move(warnings);
    return parsex::json_contract::wrapEnvelope("parseReport", std::move(payload));
}
inline nlohmann::json buildJsonValidate() {
    return ValidationResult{}.toJson();
}
inline nlohmann::json buildJsonValidate(const ValidationResult& result, bool strict) {
    auto envelope = result.toJson();
    bool passed = Validator::overallPassed(result, strict ? StrictMode::Strict : StrictMode::Lenient);
    envelope["payload"]["passed"] = passed;
    return envelope;
}
inline nlohmann::json buildJsonDiff() {
    return DiffReport{}.toJson();
}
inline nlohmann::json buildJsonDiff(const DiffReport& report) {
    return report.toJson();
}
inline nlohmann::json buildJsonWrite(const std::string& input) {
    return parsex::json_contract::wrapEnvelope("writeResult", {{"input", input}});
}
inline nlohmann::json buildJsonWrite(const std::filesystem::path& input, const std::filesystem::path& output, bool applied, bool success) {
    nlohmann::json payload;
    payload["input"] = input.string();
    payload["output"] = output.string();
    payload["applied"] = applied;
    payload["success"] = success;
    return parsex::json_contract::wrapEnvelope("writeResult", std::move(payload));
}

struct CliOptions {
    bool jsonOutput{false};
    bool noColor{false};
    bool trace{false};
};

inline void emitTraceIfEnabled(const CliOptions& opts) {
    if (opts.trace) {
        std::cerr << parsex::telemetry::TelemetryContext::currentTraceAsJson().dump(2) << "\n";
    }
}

// Call sites pass (cache dir, input) in documented order; names differ by role.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
inline void runParseCommand(const CliOptions& opts, int argc, char const* const* argv,
                            const std::string& schemaCacheDir, const std::string& parseInput) {
    applySchemaCacheDir(argc, argv, schemaCacheDir);
    parsex::telemetry::TelemetryConfig::setEnabled(opts.trace);
    if (opts.trace) {
        parsex::telemetry::TelemetryContext::reset();
    }
    ParsedFile parsedFile;
    std::exception_ptr pendingException;
    {
        parsex::telemetry::ScopedSpan span("parsex-cli.parse");
        try {
            Parser parser;
            parsedFile = parser.parseFile(std::filesystem::path(parseInput));
        } catch (...) {
            pendingException = std::current_exception();
        }
    }
    if (pendingException) {
        emitTraceIfEnabled(opts);
        std::rethrow_exception(pendingException);
    }
    if (opts.jsonOutput) {
        std::cout << buildJsonParse(parsedFile).dump(2) << "\n";
    } else {
        std::cout << formatHumanParse(parsedFile);
    }
    emitTraceIfEnabled(opts);
}

// Call sites pass (cache dir, input) in documented order; names differ by role.
inline void runValidateCommand(const CliOptions& opts, int argc, char const* const* argv,
                               const std::string& schemaCacheDir, const std::string& validateInput,
                               bool validateStrict) {
    applySchemaCacheDir(argc, argv, schemaCacheDir);
    parsex::telemetry::TelemetryConfig::setEnabled(opts.trace);
    if (opts.trace) {
        parsex::telemetry::TelemetryContext::reset();
    }
    Parser parser;
    ParsedProject project;
    ValidationResult result;
    const bool strict = validateStrict;
    bool parsedOk = false;
    {
        parsex::telemetry::ScopedSpan span("parsex-cli.validate");
        try {
            auto parsed = parser.parseFile(std::filesystem::path(validateInput));
            project.files.push_back(std::move(parsed));
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
                    auto combined = validator.validateAll(project, resolved.schema.schemaHandle.get());
                    result.merge(combined);
                } else {
                    result.errors.push_back(
                        {Severity::Error, "schema.no_release", "cannot determine AUTOSAR release for validation"});
                }
            } catch (const std::exception& ex) {
                result.errors.push_back({Severity::Error, "validator.error", ex.what()});
            }
        }
    }
    if (opts.jsonOutput) {
        std::cout << buildJsonValidate(result, strict).dump(2) << "\n";
    } else {
        std::cout << formatHumanValidate(result, strict);
    }
    emitTraceIfEnabled(opts);
    // Validations findings do not change the process exit code — always 0
    // when the tool ran; pass/fail lives in payload.passed. Only malformed
    // invocation or I/O (handled by top-level catch) causes non-zero.
}

// Call sites pass (cache dir, base, target) in documented order; names differ by role.
inline void runDiffCommand(const CliOptions& opts, int argc, char const* const* argv,
                           const std::string& schemaCacheDir, const std::string& diffBase,
                           const std::string& diffTarget) {
    applySchemaCacheDir(argc, argv, schemaCacheDir);
    parsex::telemetry::TelemetryConfig::setEnabled(opts.trace);
    if (opts.trace) {
        parsex::telemetry::TelemetryContext::reset();
    }
    DiffReport report;
    std::exception_ptr pendingException;
    {
        parsex::telemetry::ScopedSpan span("parsex-cli.diff");
        try {
            Parser parser;
            ParsedProject oldProject;
            ParsedProject newProject;
            oldProject.files.push_back(parser.parseFile(std::filesystem::path(diffBase)));
            newProject.files.push_back(parser.parseFile(std::filesystem::path(diffTarget)));
            DiffEngine engine;
            report = engine.diff(oldProject, newProject);
        } catch (...) {
            pendingException = std::current_exception();
        }
    }
    if (pendingException) {
        emitTraceIfEnabled(opts);
        std::rethrow_exception(pendingException);
    }
    if (opts.jsonOutput) {
        std::cout << buildJsonDiff(report).dump(2) << "\n";
    } else {
        std::cout << formatHumanDiff(report);
    }
    emitTraceIfEnabled(opts);
}

// Call sites pass (cache dir, input, output) in documented order; names differ by role.
inline void runWriteCommand(const CliOptions& opts, int argc, char const* const* argv,
                            const std::string& schemaCacheDir, const std::string& writeInput,
                            const std::string& writeOutput, bool writeApply) {
    applySchemaCacheDir(argc, argv, schemaCacheDir);
    parsex::telemetry::TelemetryConfig::setEnabled(opts.trace);
    if (opts.trace) {
        parsex::telemetry::TelemetryContext::reset();
    }
    bool wrote = false;
    bool success = true;
    const std::filesystem::path outPath(writeOutput);
    const std::filesystem::path inPath(writeInput);
    std::exception_ptr pendingException;
    {
        parsex::telemetry::ScopedSpan span("parsex-cli.write");
        try {
            Parser parser;
            ParsedProject project;
            project.files.push_back(parser.parseFile(inPath));
            const bool applied = writeApply;
            wrote = applied;
            if (applied) {
                WriteEngine engine;
                engine.write(project, outPath);
            } else {
                WriteEngine engine;
                const ValidationResult problems = engine.validate(project);
                success = !problems.hasErrors();
            }
        } catch (...) {
            pendingException = std::current_exception();
        }
    }
    if (pendingException) {
        emitTraceIfEnabled(opts);
        std::rethrow_exception(pendingException);
    }
    if (opts.jsonOutput) {
        std::cout << buildJsonWrite(inPath, outPath, wrote, success).dump(2) << "\n";
    } else {
        std::cout << formatHumanWrite(inPath, outPath, wrote);
        if (!wrote && !success) {
            std::cout << "  (would fail: project has write validation errors)\n";
        }
    }
    emitTraceIfEnabled(opts);
}
// NOLINTEND(bugprone-easily-swappable-parameters)

}  // namespace

// Main wires four subcommands; per-command work lives in run*Command() helpers above.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int main(int argc, char const* const* argv)
{
    CLI::App app{"parsex - ARXML CAN parsing, validation, diffing and safe editing", "parsex"};

    // NOTE: set_version_flag on the top-level app only fires at the top level
    // by default (e.g. `parsex --version` works; `parsex parse --version` does
    // not unless each subcommand also sets its own version flag).
    app.set_version_flag("--version,-V", std::string(libparsexVersion()));

    // Shared top-level options readable from every subcommand callback.
    // PAR-206 branches on jsonOutput without re-parsing.
    CliOptions opts;
    app.add_flag("--json", opts.jsonOutput, "Emit machine-readable JSON instead of human-readable text");
    app.add_flag("--no-color", opts.noColor, "Disable colored output");
    // PAR-227: top-level --trace flag enabling telemetry span emission per PAR-178.
    app.add_flag("--trace", opts.trace, "Emit telemetryReport JSON to stderr");
    // NOTE (PAR-212): CLI11 resolves `--json` before the subcommand
    // (`parsex --json parse ...`). `parsex parse --json ...` is rejected
    // unless each subcommand re-declares the flag or fallthrough is enabled;
    // single top-level flag kept intentionally, documented here.
    // NOTE (PAR-214): human formatters gate any future ANSI codes behind
    // parsex::cli::colorEnabled(opts.noColor); no colored output exists yet.

    // PAR-221: first real config-driven option, demonstrating the full
    // flag > env > config > default chain. Config key is `schema-cache-dir`
    // (matches the flag name minus leading --).
    std::string schemaCacheDir;
    app.add_option("--schema-cache-dir", schemaCacheDir, "Directory for cached AUTOSAR schemas");

    auto* parseCmd = app.add_subcommand("parse", "Parse an ARXML file and report its structure");
    auto* validateCmd = app.add_subcommand("validate", "Validate an ARXML file against ParseX rules (exit 0 on run; check payload.passed / human summary for findings; --strict makes warnings fail)");
    auto* diffCmd = app.add_subcommand("diff", "Diff two ARXML files");
    auto* writeCmd = app.add_subcommand("write", "Apply a safe edit to an ARXML file (dry-run by default; use --apply to write)");

    // Shared input-path options with CLI11 built-in existence validation.
    // Bad paths fail fast at parse time before any engine code runs. Error
    // text from CLI11 already names the flag and the bad value; any further
    // clig.dev tone polish belongs to PAR-204, not here.
    std::string parseInput;
    std::string validateInput;
    bool validateStrict{false};
    std::string writeInput;
    std::string writeOutput;
    bool writeApply{false};
    std::string diffBase;
    std::string diffTarget;
    parseCmd->add_option("--input,-i", parseInput, "Input ARXML file")->required()->check(CLI::ExistingFile);
    validateCmd->add_option("--input,-i", validateInput, "Input ARXML file")->required()->check(CLI::ExistingFile);
    // PAR-225: --strict sequences Parser before Validator per lld.md (validate --strict, then parse).
    // Exit-code convention: validate findings never cause non-zero exit; pass/fail lives in the report
    // body/payload (see --help). Scripts should check payload.passed, not the process exit code.
    validateCmd->add_flag("--strict", validateStrict, "Strict mode: warnings fail the overall verdict");
    writeCmd->add_option("--input,-i", writeInput, "Input ARXML file")->required()->check(CLI::ExistingFile);
    // PAR-226: write output and safety. Dry-run unless --apply, so an accidental write never mutates a file.
    writeCmd->add_option("--output,-o", writeOutput, "Output ARXML file")->required();
    writeCmd->add_flag("--apply", writeApply, "Actually write the file (without this, dry-run only)");
    diffCmd->add_option("--base", diffBase, "Base ARXML file")->required()->check(CLI::ExistingFile);
    diffCmd->add_option("--target", diffTarget, "Target ARXML file")->required()->check(CLI::ExistingFile);
    parseCmd->callback([&]() {
        runParseCommand(opts, argc, argv, schemaCacheDir, parseInput);
    });
    validateCmd->callback([&]() {
        runValidateCommand(opts, argc, argv, schemaCacheDir, validateInput, validateStrict);
    });
    diffCmd->callback([&]() {
        runDiffCommand(opts, argc, argv, schemaCacheDir, diffBase, diffTarget);
    });
    writeCmd->callback([&]() {
        runWriteCommand(opts, argc, argv, schemaCacheDir, writeInput, writeOutput, writeApply);
    });
    (void)parseCmd;
    (void)validateCmd;
    (void)diffCmd;
    (void)writeCmd;

    // PAR-220: CLI11 built-in config-file support (TOML-like). Empty default
    // (not required) so the tool works without one. Leave
    // allow_config_extras() at CLI11's default (Ignore — unknown keys are
    // silently ignored, not errored). Earlier doc assumed default errors, but
    // CLI11 2.x defaults to Ignore; the choice is reified with a test in
    // PAR-222, including the misspelled-key sharp edge.
    // Known sharp edge: a typo like `schema-cach-dir` in the config file is
    // silently ignored (no warning), so the misspelled option has no effect.
    // Fixing this would require custom validation beyond CLI11's built-in
    // handling, out of scope for this Story — documented here instead.
    app.set_config("--config", "", "Read a config file", false);

    app.require_subcommand(1);

    // PAR-218: single top-level error boundary per clig.dev. CLI11 parse
    // failures are delegated to app.exit() (which handles --help/--version's
    // Success paths correctly); everything else is rewritten to one
    // `parsex: <message>` line on stderr and mapped via classify().
    // Error output always goes to stderr, even under --json; --json failure
    // stays plain-text on stderr (JSON error envelope is a documented future
    // enhancement, not this Story).
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& err) {
        return app.exit(err);
    } catch (const std::exception& ex) {
        // Keep the message on one line; collapse any embedded newlines so the
        // boundary never emits multi-line diagnostics.
        std::string msg = ex.what();
        for (char& currentChar : msg) {
            if (currentChar == '\n' || currentChar == '\r') {
                currentChar = ' ';
            }
        }
        // Prefix with program name per clig.dev error guidance; avoid raw
        // exception type dumps when the existing what() is already friendly.
        std::cerr << "parsex: " << msg << "\n";
        return static_cast<int>(parsex::cli::classify(ex));
    }
    return 0;
}
