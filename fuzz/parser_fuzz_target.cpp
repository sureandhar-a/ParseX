// Direct-bytes fuzz entry for the parsing pipeline: hands fuzzer-mutated
// bytes straight to the in-memory entry point, proving no input crashes,
// hangs, or trips a sanitizer. Any exception is an expected outcome for
// mutated input — only a crash/hang/sanitizer report fails the run.
//
// Build (requires Clang with the runtime; on macOS that is Homebrew LLVM,
// NOT Xcode CLT):
//   cmake -S . -B build/fuzz \
//     -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
//     -DPARSEX_ENABLE_FUZZING=ON -DCMAKE_BUILD_TYPE=Debug
//   cmake --build build/fuzz --target parsex_fuzz_parser
// Run (short bounded pass; corpus seeded from tests/fixtures). Pass a scratch
// dir FIRST so newly-found mutants go there instead of back into the
// committed seed corpus (first positional dir doubles as output):
//   mkdir -p /tmp/fuzzwork && ASAN_OPTIONS=detect_leaks=0 \
//     build/fuzz/fuzz/parsex_fuzz_parser /tmp/fuzzwork fuzz/corpus/parser \
//     -max_len=65536 -max_total_time=180
// Leak note: run fuzzing with leak detection only on Linux CI
// (-detect_leaks=1); Apple Silicon noise makes local leak reports
// untrustworthy, so detect_leaks=0 locally.

#include <parsex/parser/parser.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0 || size > 262144) {
        return 0;
    }
    try {
        Parser{}.parseBytes({data, size});
    } catch (const std::exception&) {
        // Any parse failure is a normal outcome for mutated bytes.
    }
    return 0;
}
