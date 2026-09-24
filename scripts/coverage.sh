#!/bin/sh
# One-command local coverage: configure with instrumentation, build, render.
# Needs: gcovr (GCC) or llvm-cov/llvm-profdata (Clang). See docs/testing.md.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build/coverage"
cmake -S "${ROOT}" -B "${BUILD}" -DPARSEX_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build "${BUILD}" -j 4
cmake --build "${BUILD}" --target coverage
echo "Open ${BUILD}/coverage/index.html"
