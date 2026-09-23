#pragma once

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
