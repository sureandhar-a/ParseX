#pragma once

#include <exception>
#include <filesystem>
#include <ios>
#include <stdexcept>
#include <string_view>

#include "CLI/CLI.hpp"
#include "parsex/parser/parse_error.hpp"
#include "parsex/parser/project_error.hpp"
#include "parsex/parser/release_error.hpp"
#include "parsex/schema/schema_resolution_error.hpp"
#include "parsex/write/write_engine.hpp"

// PAR-216: small, documented process exit-code convention for parsex-cli,
// adapted from BSD sysexits.h rather than inventing bespoke numbering so
// scripts get conventional, greppable statuses.
//
// Intentionally a SUBSET of sysexits.h (not the full list): from the CLI's
// vantage point only these four failure modes are distinguishable —
//   Usage    (EX_USAGE 64): bad CLI invocation (CLI11 parse failure,
//              missing required option, unknown subcommand).
//   DataErr  (EX_DATAERR 65): the ARXML input itself is invalid / fails
//              validation (malformed XML, schema/reference/semantic errors).
//   IoErr    (EX_IOERR 74): a file could not be read or written
//              (unreadable input bypassing CLI::ExistingFile, unwritable
//              output, schema cache I/O).
//   Software (EX_SOFTWARE 70): unexpected internal error — anything else is
//              a bug and should be rare.
// Other sysexits.h values (EX_NOHOST, EX_UNAVAILABLE, etc.) have no
// CLI-observable trigger in ParseX and are deliberately omitted.

enum class ExitCode : int {
    Ok = 0,
    Usage = 64,
    DataErr = 65,
    IoErr = 74,
    Software = 70
};

namespace parsex::cli {

// PAR-217: inventory of libparsex throws (PAR-65/PAR-92/PAR-138):
//   ParseError(Io) -> IoErr; ParseError(Syntax) -> DataErr
//   UnsupportedReleaseError, DanglingFileReferenceError, WriteError -> DataErr
//   SchemaResolutionError(Missing) -> IoErr; (Corrupt/Unsupported) -> DataErr
//   std::filesystem::filesystem_error, std::ios_base::failure -> IoErr
//   CLI::ParseError -> Usage; anything else -> Software.
// Ordered most-specific-first when used as a catch chain; classify() mirrors
// that order with dynamic_casts.
inline ExitCode classify(const std::exception& ex) {
    if (dynamic_cast<const CLI::ParseError*>(&ex) != nullptr) {
        return ExitCode::Usage;
    }
    if (const auto* parseErr = dynamic_cast<const ParseError*>(&ex)) {
        return parseErr->reason() == ParseErrorReason::Io ? ExitCode::IoErr : ExitCode::DataErr;
    }
    if (const auto* schemaErr = dynamic_cast<const SchemaResolutionError*>(&ex)) {
        return schemaErr->reason() == SchemaResolutionReason::SchemaFileMissing
                   ? ExitCode::IoErr
                   : ExitCode::DataErr;
    }
    if (dynamic_cast<const UnsupportedReleaseError*>(&ex) != nullptr) {
        return ExitCode::DataErr;
    }
    if (dynamic_cast<const DanglingFileReferenceError*>(&ex) != nullptr) {
        return ExitCode::DataErr;
    }
    if (dynamic_cast<const WriteError*>(&ex) != nullptr) {
        return ExitCode::DataErr;
    }
    if (dynamic_cast<const std::filesystem::filesystem_error*>(&ex) != nullptr) {
        return ExitCode::IoErr;
    }
    if (dynamic_cast<const std::ios_base::failure*>(&ex) != nullptr) {
        return ExitCode::IoErr;
    }
    // Heuristic: libparsex file-I/O failures often surface as runtime_error
    // with messages like "failed to write" / "failed to move" (write path).
    // Treat those as IoErr for script-friendly codes rather than Software.
    {
        std::string_view msg = ex.what();
        if (msg.find("failed to write") != std::string_view::npos ||
            msg.find("failed to move") != std::string_view::npos) {
            return ExitCode::IoErr;
        }
    }
    return ExitCode::Software;
}

}  // namespace parsex::cli
