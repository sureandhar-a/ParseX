#include <iostream>
#include <string>
#include "CLI/CLI.hpp"
#include "parsex/version.hpp"

using namespace std;

int main(int argc, char const *argv[])
{
    CLI::App app{"parsex - ARXML CAN parsing, validation, diffing and safe editing", "parsex"};

    // NOTE: set_version_flag on the top-level app only fires at the top level
    // by default (e.g. `parsex --version` works; `parsex parse --version` does
    // not unless each subcommand also sets its own version flag).
    // TODO(PAR-207): document --version in docs/cli.md
    app.set_version_flag("--version,-V", std::string(libparsexVersion()));

    auto* parseCmd = app.add_subcommand("parse", "Parse an ARXML file and report its structure");
    auto* validateCmd = app.add_subcommand("validate", "Validate an ARXML file against ParseX rules");
    auto* diffCmd = app.add_subcommand("diff", "Diff two ARXML files");
    auto* writeCmd = app.add_subcommand("write", "Apply a safe edit to an ARXML file");
    (void)parseCmd;
    (void)validateCmd;
    (void)diffCmd;
    (void)writeCmd;

    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);
    return 0;
}
