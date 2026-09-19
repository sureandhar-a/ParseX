// Atomic-write tests: a written entry reads back byte-for-byte (including
// embedded NULs), and no .tmp.* stray files remain after a successful write.

#include <gtest/gtest.h>
#include <parsex/schema/atomic_write.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::filesystem::path testScratchDir() {
    return std::filesystem::temp_directory_path() / "parsex_atomic_write_test";
}

std::string readFileBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool hasTmpStrays(const std::filesystem::path& dir) {
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().filename().string().find(".tmp.") != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(AtomicWriteTest, WrittenEntryReadsBackByteForByte) {
    const auto dir = testScratchDir();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);

    const std::string payload =
        std::string("<?xml version=\"1.0\"?>\n<xs:schema>\n") +
        std::string("\0binary\0safety\0", 15) +
        std::string(10'000, 'x');

    const auto target = dir / "R21-11.xsd.cache";
    writeCacheAtomically(target, payload);

    EXPECT_EQ(readFileBytes(target), payload);

    std::filesystem::remove_all(dir, ec);
}

TEST(AtomicWriteTest, NoTmpFilesLeftAfterSuccessfulWrite) {
    const auto dir = testScratchDir();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);

    writeCacheAtomically(dir / "4.2.2.xsd.cache", "<schema>first</schema>");
    writeCacheAtomically(dir / "4.2.2.xsd.cache", "<schema>second</schema>");

    EXPECT_FALSE(hasTmpStrays(dir));
    EXPECT_EQ(readFileBytes(dir / "4.2.2.xsd.cache"), "<schema>second</schema>");

    std::filesystem::remove_all(dir, ec);
}
