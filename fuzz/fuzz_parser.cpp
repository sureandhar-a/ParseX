// libFuzzer harness for the Parser: feeds fuzzer-mutated bytes through
// Parser::parseFile() and proves no input crashes, hangs, or trips a
// sanitizer. parseFile() takes a path (not bytes), so each input goes via a
// uniquely-named temp file (mkstemp: safe under parallel -workers runs).
// Every exception type is an expected outcome for mutated input — only a
// crash/hang/sanitizer report fails the run.
//
// Build (requires Clang with the libFuzzer runtime; on macOS that is Homebrew
// LLVM, NOT Xcode CLT):
//   cmake -S . -B build/fuzz \
//     -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
//     -DPARSEX_ENABLE_FUZZING=ON -DCMAKE_BUILD_TYPE=Debug
//   cmake --build build/fuzz --target fuzz_parser
// Run (short bounded pass; corpus seeded from tests/fixtures). Pass a scratch
// dir FIRST so libFuzzer writes newly-found mutants there instead of back
// into the committed seed corpus (its first positional dir doubles as output):
//   mkdir -p /tmp/fuzzwork && ASAN_OPTIONS=detect_leaks=0 \
//     build/fuzz/fuzz/fuzz_parser /tmp/fuzzwork fuzz/corpus/parser \
//     -max_len=65536 -max_total_time=180
// Leak note: run fuzzing with leak detection only on Linux CI
// (-detect_leaks=1); Apple Silicon LSan noise makes local leak reports
// untrustworthy, so detect_leaks=0 locally.

#include <parsex/parser/parser.hpp>

#include <unistd.h>

#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

std::filesystem::path writeTempInput(const unsigned char* data, std::size_t size) {
    std::string pattern =
        (std::filesystem::temp_directory_path() / "parsex_fuzz_XXXXXX").string();
    const int fd = mkstemp(pattern.data());
    if (fd < 0) {
        return {};
    }
    std::size_t written = 0;
    while (written < size) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): fd I/O needs a raw offset pointer.
        const auto chunk = write(fd, data + written, size - written);
        if (chunk <= 0) {
            break;
        }
        written += static_cast<std::size_t>(chunk);
    }
    close(fd);
    return pattern;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, std::size_t size) {
    if (size == 0 || size > 262144) {
        return 0;
    }
    const std::filesystem::path input = writeTempInput(data, size);
    if (input.empty()) {
        return 0;
    }
    try {
        Parser{}.parseFile(input);
    } catch (const std::exception&) {
        // Any parse failure is a normal outcome for mutated bytes.
    }
    std::error_code ignored;
    std::filesystem::remove(input, ignored);
    return 0;
}
