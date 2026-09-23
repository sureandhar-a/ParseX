#include <iostream>
#include "CLI/CLI.hpp"

using namespace std;

int main(int argc, char const *argv[])
{
    CLI::App app{"parsex - ARXML CAN parsing, validation, diffing and safe editing", "parsex"};

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
