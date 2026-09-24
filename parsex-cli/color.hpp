#pragma once

#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <io.h>
#define PARSEX_ISATTY _isatty
#define PARSEX_FILENO _fileno
#else
#include <unistd.h>
#define PARSEX_ISATTY isatty
#define PARSEX_FILENO fileno
#endif

#include <cstdio>

// PAR-214: single decision point gating any colored human-readable output,
// per clig.dev/no-color.org conventions. PAR-206 consumes this; no colored
// output exists yet.
namespace parsex::cli {

// Pure, unit-testable core: no env/TTY queries here.
inline bool colorEnabled(bool noColorFlag, bool isTty, bool hasNoColorEnv) {
    if (noColorFlag) {
        return false;
    }
    if (hasNoColorEnv) {
        return false;
    }
    if (!isTty) {
        return false;
    }
    return true;
}

// Real query: --no-color flag + NO_COLOR env (presence disables; non-empty is
// enough, content is never inspected) + isatty(stdout).
inline bool colorEnabled(bool noColorFlag) {
    if (noColorFlag) {
        return false;
    }
    if (std::getenv("NO_COLOR") != nullptr) {
        return false;
    }
    return PARSEX_ISATTY(PARSEX_FILENO(stdout)) != 0;
}

}  // namespace parsex::cli
