#pragma once

#include <filesystem>
#include <string>

namespace detail {

// Testing seam and shared implementation stages for writeCacheAtomically().
// Production code must call writeCacheAtomically(); tests use these to
// simulate dying mid-write (write the temp file, then stop before rename).

// Builds a fresh temp path "<filename>.tmp.<random-hex>" inside dir.
std::filesystem::path makeTmpPath(
    const std::filesystem::path& dir, const std::string& filename);

// Opens/creates tmpPath, writes every byte of data, fsyncs, closes.
// Throws std::system_error on failure. Performs NO rename.
void writeTmpFileSync(const std::filesystem::path& tmpPath, const std::string& data);

}  // namespace detail
