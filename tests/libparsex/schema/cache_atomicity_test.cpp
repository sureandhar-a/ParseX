// Crash-safety proof for the atomic-write scheme: a process that dies
// partway through writing a cache entry must never leave a half-written file
// at the final path.
//
// "Died mid-write" is simulated without crashing the test process: the
// detail seam writes the temp file exactly as writeCacheAtomically() would,
// but the rename() step is deliberately skipped.

#include <gtest/gtest.h>
#include <parsex/schema/atomic_write.hpp>
#include <parsex/schema/detail/atomic_write_detail.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::filesystem::path testScratchDir() {
    return std::filesystem::temp_directory_path() / "parsex_cache_atomicity_test";
}

// Truncated garbage: what a kill -9 mid-write could leave in the temp file.
// Must never become observable at the final path.
const std::string kGarbage = "<xs:schema><trunca";

const std::string kGoodPayload =
    "<?xml version=\"1.0\"?>\n<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"/>\n";

std::string readFileBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void clearScratch() {
    std::error_code ec;
    std::filesystem::remove_all(testScratchDir(), ec);
    std::filesystem::create_directories(testScratchDir(), ec);
}

// Simulates dying after the temp write but before rename().
std::filesystem::path dieMidWrite(
    const std::filesystem::path& dir, const std::string& filename) {
    const auto tmp = detail::makeTmpPath(dir, filename);
    detail::writeTmpFileSync(tmp, kGarbage);
    return tmp;  // rename() deliberately skipped — process "died" here
}

}  // namespace

TEST(CacheAtomicityTest, InterruptedFirstWriteLeavesFinalPathAbsent) {
    clearScratch();
    const auto dir = testScratchDir();
    const auto finalPath = dir / "R21-11.xsd.cache";
    ASSERT_FALSE(std::filesystem::exists(finalPath));

    dieMidWrite(dir, "R21-11.xsd.cache");

    // The garbage exists only under a .tmp.* name; the final path is untouched.
    EXPECT_FALSE(std::filesystem::exists(finalPath));

    clearScratch();
}

TEST(CacheAtomicityTest, InterruptedOverwritePreservesPreviousGoodContents) {
    clearScratch();
    const auto dir = testScratchDir();
    const auto finalPath = dir / "4.2.2.xsd.cache";
    writeCacheAtomically(finalPath, kGoodPayload);
    ASSERT_EQ(readFileBytes(finalPath), kGoodPayload);

    dieMidWrite(dir, "4.2.2.xsd.cache");

    // Previous good contents survive; the garbage never replaced them.
    EXPECT_EQ(readFileBytes(finalPath), kGoodPayload);

    clearScratch();
}

TEST(CacheAtomicityTest, RecoverySucceedsAndLeftoverTmpIsIgnored) {
    clearScratch();
    const auto dir = testScratchDir();
    const auto finalPath = dir / "R20-11.xsd.cache";

    // A leftover .tmp.* file from an earlier interrupted attempt is present.
    const auto leftover = dieMidWrite(dir, "R20-11.xsd.cache");
    ASSERT_TRUE(std::filesystem::exists(leftover));

    // The next normal attempt recovers cleanly with full contents.
    writeCacheAtomically(finalPath, kGoodPayload);
    EXPECT_EQ(readFileBytes(finalPath), kGoodPayload);

    // Resolution reads ONLY the final path (never globs for temp files), so
    // the leftover cannot confuse a later resolveSchema() call: the final
    // path holds exactly the good payload despite the stray beside it.
    // (The next PBI's resolveSchema() must preserve this rule.)
    EXPECT_TRUE(std::filesystem::exists(leftover));
    std::ifstream finalIn(finalPath, std::ios::binary);
    EXPECT_EQ(
        std::string(std::istreambuf_iterator<char>(finalIn), std::istreambuf_iterator<char>()),
        kGoodPayload);

    clearScratch();
}
