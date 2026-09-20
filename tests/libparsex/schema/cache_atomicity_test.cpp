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
// Must never become observable at the final path. Function-local statics
// keep dynamic initialization out of namespace scope.
const std::string& garbagePayload() {
    static const std::string value = "<xs:schema><trunca";
    return value;
}

const std::string& goodPayload() {
    static const std::string value =
        "<?xml version=\"1.0\"?>\n<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"/>\n";
    return value;
}

std::string readFileBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void clearScratch() {
    std::error_code errCode;
    std::filesystem::remove_all(testScratchDir(), errCode);
    std::filesystem::create_directories(testScratchDir(), errCode);
}

// Simulates dying after the temp write but before rename().
std::filesystem::path dieMidWrite(
    const std::filesystem::path& dir, const std::string& filename) {
    const auto tmp = detail::makeTmpPath(dir, filename);
    detail::writeTmpFileSync(tmp, garbagePayload());
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
    writeCacheAtomically(finalPath, goodPayload());
    ASSERT_EQ(readFileBytes(finalPath), goodPayload());

    dieMidWrite(dir, "4.2.2.xsd.cache");

    // Previous good contents survive; the garbage never replaced them.
    EXPECT_EQ(readFileBytes(finalPath), goodPayload());

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
    writeCacheAtomically(finalPath, goodPayload());
    EXPECT_EQ(readFileBytes(finalPath), goodPayload());

    // Resolution reads ONLY the final path (never globs for temp files), so
    // the leftover cannot confuse a later resolveSchema() call: the final
    // path holds exactly the good payload despite the stray beside it.
    // (The next PBI's resolveSchema() must preserve this rule.)
    EXPECT_TRUE(std::filesystem::exists(leftover));
    std::ifstream finalInput(finalPath, std::ios::binary);
    const std::string finalBytes{
        std::istreambuf_iterator<char>(finalInput), std::istreambuf_iterator<char>()};
    EXPECT_EQ(finalBytes, goodPayload());

    clearScratch();
}
