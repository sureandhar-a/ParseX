#pragma once

#include <filesystem>
#include <string>

// Atomically writes data to path: bytes go to a temp file in the same
// directory (same filesystem, which rename()'s atomicity depends on), the
// file is fsync'd, then rename()d over the destination — so a reader opening
// path never observes a half-written state, even if this process is killed
// mid-write. Any failure (write/fsync/rename) removes the temp file and
// throws std::system_error; nothing stray is left behind.
//
// Durability nuance (documented, not solved): rename() alone does not fsync
// the *directory entry*, so a power loss right after return could lose the
// entry on some filesystems. That is acceptable here — a lost schema cache
// entry is always regenerable from the user-supplied source file.
void writeCacheAtomically(const std::filesystem::path& path, const std::string& data);
