#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include "CLI/CLI.hpp"
#include <nlohmann/json.hpp>
#include "parsex/diff/diff_report.hpp"
#include "parsex/json_contract/envelope.hpp"
#include "parsex/parser/parser.hpp"
#include "parsex/schema/cache_layout.hpp"
#include "parsex/validator/validation_result.hpp"
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
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--schema-cache-dir" || arg.rfind("--schema-cache-dir=", 0) == 0) {
            flagPresent = true;
            break;
        }
    }
    if (flagPresent) {
        return {std::filesystem::path(boundValue), "flag"};
    }
    if (const char* env = std::getenv("PARSEX_SCHEMA_CACHE_DIR"); env != nullptr && env[0] != '\0') {
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
inline std::string formatHumanParse(const ParsedFile& pf) {
    std::ostringstream oss;
    oss << "Parsed " << pf.sourcePath.string() << "\n";
    oss << "  release: " << (pf.autosarRelease.empty() ? "(unknown)" : pf.autosarRelease) << "\n";
    oss << "  clusters: " << pf.clusters.size() << "\n";
    oss << "  ecuInstances: " << pf.ecuInstances.size() << "\n";
    oss << "  frames: " << pf.frames.size() << "\n";
    oss << "  pdus: " << pf.pdus.size() << "\n";
    oss << "  signals: " << pf.signals.size() << "\n";
    oss << "  signalGroups: " << pf.signalGroups.size() << "\n";
    if (!pf.warnings.empty()) {
        oss << "  warnings: " << pf.warnings.size() << "\n";
        for (const auto& w : pf.warnings) {
            oss << "    - " << w.message << "\n";
        }
    }
    return oss.str();
}

inline std::string formatHumanValidate(const std::string& input) {
    return "Validated " + input + ": OK (placeholder)";
}
inline std::string formatHumanDiff(const std::string& base, const std::string& target) {
    return "Diff " + base + " vs " + target + ": no differences (placeholder)";
}
inline std::string formatHumanWrite(const std::string& input) {
    return "Wrote " + input + " (placeholder, dry-run)";
}

// PAR-224: JSON via shared envelope — parseReport payload with counts.
inline nlohmann::json buildJsonParse(const ParsedFile& pf) {
    nlohmann::json payload;
    payload["sourcePath"] = pf.sourcePath.string();
    payload["autosarRelease"] = pf.autosarRelease;
    payload["counts"] = {{"clusters", pf.clusters.size()},
                         {"ecuInstances", pf.ecuInstances.size()},
                         {"frames", pf.frames.size()},
                         {"pdus", pf.pdus.size()},
                         {"signals", pf.signals.size()},
                         {"signalGroups", pf.signalGroups.size()}};
    nlohmann::json warnings = nlohmann::json::array();
    for (const auto& w : pf.warnings) {
        warnings.push_back({{"message", w.message}});
    }
    payload["warnings"] = std::move(warnings);
    return parsex::json_contract::wrapEnvelope("parseReport", std::move(payload));
}
inline nlohmann::json buildJsonValidate() {
    return ValidationResult{}.toJson();
}
inline nlohmann::json buildJsonDiff() {
    return DiffReport{}.toJson();
}
inline nlohmann::json buildJsonWrite(const std::string& input) {
    return parsex::json_contract::wrapEnvelope("writeResult", {{"input", input}});
}

}  // namespace

int main(int argc, char const *argv[])
{
    CLI::App app{"parsex - ARXML CAN parsing, validation, diffing and safe editing", "parsex"};

    // NOTE: set_version_flag on the top-level app only fires at the top level
    // by default (e.g. `parsex --version` works; `parsex parse --version` does
    // not unless each subcommand also sets its own version flag).
    // TODO(PAR-207): document --version in docs/cli.md
    app.set_version_flag("--version,-V", std::string(libparsexVersion()));

    // Shared top-level options readable from every subcommand callback.
    // PAR-206 branches on jsonOutput without re-parsing.
    struct CliOptions {
        bool jsonOutput{false};
        bool noColor{false};
    };
    CliOptions opts;
    app.add_flag("--json", opts.jsonOutput, "Emit machine-readable JSON instead of human-readable text");
    app.add_flag("--no-color", opts.noColor, "Disable colored output");
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
    auto* validateCmd = app.add_subcommand("validate", "Validate an ARXML file against ParseX rules");
    auto* diffCmd = app.add_subcommand("diff", "Diff two ARXML files");
    auto* writeCmd = app.add_subcommand("write", "Apply a safe edit to an ARXML file");

    // Shared input-path options with CLI11 built-in existence validation.
    // Bad paths fail fast at parse time before any engine code runs. Error
    // text from CLI11 already names the flag and the bad value; any further
    // clig.dev tone polish belongs to PAR-204, not here.
    std::string parseInput;
    std::string validateInput;
    std::string writeInput;
    std::string diffBase;
    std::string diffTarget;
    parseCmd->add_option("--input,-i", parseInput, "Input ARXML file")->required()->check(CLI::ExistingFile);
    validateCmd->add_option("--input,-i", validateInput, "Input ARXML file")->required()->check(CLI::ExistingFile);
    writeCmd->add_option("--input,-i", writeInput, "Input ARXML file")->required()->check(CLI::ExistingFile);
    diffCmd->add_option("--base", diffBase, "Base ARXML file")->required()->check(CLI::ExistingFile);
    diffCmd->add_option("--target", diffTarget, "Target ARXML file")->required()->check(CLI::ExistingFile);
    parseCmd->callback([&]() {
        applySchemaCacheDir(argc, argv, schemaCacheDir);
        Parser parser;
        ParsedFile pf = parser.parseFile(std::filesystem::path(parseInput));
        if (opts.jsonOutput) {
            std::cout << buildJsonParse(pf).dump(2) << "\n";
        } else {
            std::cout << formatHumanParse(pf);
        }
    });
    validateCmd->callback([&]() {
        applySchemaCacheDir(argc, argv, schemaCacheDir);
        if (opts.jsonOutput) {
            std::cout << buildJsonValidate().dump(2) << "\n";
        } else {
            std::cout << formatHumanValidate(validateInput) << "\n";
        }
    });
    diffCmd->callback([&]() {
        applySchemaCacheDir(argc, argv, schemaCacheDir);
        if (opts.jsonOutput) {
            std::cout << buildJsonDiff().dump(2) << "\n";
        } else {
            std::cout << formatHumanDiff(diffBase, diffTarget) << "\n";
        }
    });
    writeCmd->callback([&]() {
        applySchemaCacheDir(argc, argv, schemaCacheDir);
        if (opts.jsonOutput) {
            std::cout << buildJsonWrite(writeInput).dump(2) << "\n";
        } else {
            std::cout << formatHumanWrite(writeInput) << "\n";
        }
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
        for (char& ch : msg) {
            if (ch == '\n' || ch == '\r') {
                ch = ' ';
            }
        }
        // Prefix with program name per clig.dev error guidance; avoid raw
        // exception type dumps when the existing what() is already friendly.
        std::cerr << "parsex: " << msg << "\n";
        return static_cast<int>(parsex::cli::classify(ex));
    }
    return 0;
}
