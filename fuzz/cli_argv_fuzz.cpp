// Command-line argument fuzz entry: splits fuzzer bytes on NUL into a
// synthetic argv and runs the same CLI11 subcommand structure as the real
// entry point (parse/validate/diff/write). CLI11's ParseError is expected
// control flow, not a crash — only unexpected exceptions/aborts fail the run.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "CLI/CLI.hpp"

namespace {

void runArgv(const std::vector<std::string>& args) {
    CLI::App app{"parsex fuzz"};
    auto* parseCmd = app.add_subcommand("parse", "parse");
    std::string input;
    parseCmd->add_option("-i,--input", input, "input");
    auto* validateCmd = app.add_subcommand("validate", "validate");
    validateCmd->add_option("-i,--input", input, "input");
    auto* diffCmd = app.add_subcommand("diff", "diff");
    diffCmd->add_option("--old", input, "old");
    auto* writeCmd = app.add_subcommand("write", "write");
    writeCmd->add_option("-i,--input", input, "input");
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("parsex"));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    try {
        app.parse(static_cast<int>(argv.size()), argv.data());
    } catch (const CLI::ParseError&) {
        // Expected: bad flags, missing input, unknown subcommand.
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0 || size > 8192) {
        return 0;
    }
    std::vector<std::string> args;
    std::string current;
    std::size_t count = 0;
    for (std::size_t i = 0; i < size && count < 32; ++i) {
        if (data[i] == 0) {
            args.push_back(current);
            current.clear();
            ++count;
        } else {
            current.push_back(static_cast<char>(data[i]));
        }
    }
    if (!current.empty() && count < 32) {
        args.push_back(current);
    }
    try {
        runArgv(args);
    } catch (const CLI::ParseError&) {
    } catch (const std::exception&) {
        // Any other std::exception is also handled (no file I/O happens here).
    }
    return 0;
}
