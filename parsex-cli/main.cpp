#include <iostream>
#include <string>
#include "CLI/CLI.hpp"
#include <nlohmann/json.hpp>
#include "parsex/diff/diff_report.hpp"
#include "parsex/json_contract/envelope.hpp"
#include "parsex/validator/validation_result.hpp"
#include "parsex/version.hpp"
#include "color.hpp"
#include "exit_code.hpp"

using namespace std;

namespace {

// PAR-213: human-readable placeholders (real formatting arrives in PAR-206).
inline std::string formatHumanParse(const std::string& input) {
    return "Parsed " + input + " (placeholder)";
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

// PAR-213: JSON branches via the shared Output Contract helper so
// contractVersion/toolVersion never diverge with CLI-local copies.
// validate/diff reuse the engines' own toJson() (already envelope-wrapped
// and schema-valid); parse/write use wrapEnvelope() directly with placeholder
// payloads until PAR-206 wires the real engines (schema extension for the new
// kinds tracked there).
inline nlohmann::json buildJsonParse(const std::string& input) {
    return parsex::json_contract::wrapEnvelope("parseReport", {{"input", input}});
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
        if (opts.jsonOutput) {
            std::cout << buildJsonParse(parseInput).dump(2) << "\n";
        } else {
            std::cout << formatHumanParse(parseInput) << "\n";
        }
    });
    validateCmd->callback([&]() {
        if (opts.jsonOutput) {
            std::cout << buildJsonValidate().dump(2) << "\n";
        } else {
            std::cout << formatHumanValidate(validateInput) << "\n";
        }
    });
    diffCmd->callback([&]() {
        if (opts.jsonOutput) {
            std::cout << buildJsonDiff().dump(2) << "\n";
        } else {
            std::cout << formatHumanDiff(diffBase, diffTarget) << "\n";
        }
    });
    writeCmd->callback([&]() {
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
