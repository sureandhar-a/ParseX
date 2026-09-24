#pragma once

#include <filesystem>
#include <string>

// On-disk schema cache layout (XDG Base Directory Specification):
//   $XDG_CACHE_HOME/parsex/schemas/  if XDG_CACHE_HOME is set and non-empty,
//   $HOME/.cache/parsex/schemas/     otherwise.
// The directory (and parents) is created if missing. If neither variable is
// available, the system temp directory is used as a last resort so the
// function never fails outright; write errors surface later, at cache-write
// time, not here.
std::filesystem::path getCacheDirectory();

// PAR-221: CLI-driven override for --schema-cache-dir / PARSEX_SCHEMA_CACHE_DIR /
// config file, with precedence flag > env > config > default per CLI guideline.
// When set, getCacheDirectory() returns the override instead of the XDG-derived
// default. Used by parsex-cli's main() after parsing; unset by default.
void setCacheDirectoryOverride(const std::filesystem::path& dir);
void clearCacheDirectoryOverride();

// Cache key scheme: one file per release, "<release>.xsd.cache", inside
// getCacheDirectory(). The release string is used verbatim — callers
// (resolveSchema) validate it against the known release set first.
std::filesystem::path cachePathFor(const std::string& release);
