cmake_minimum_required(VERSION 3.21)

# 1. READ VCPKG_ROOT FROM YOUR SYSTEM ENVIRONMENT
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(ParseX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_program(CLANG_TIDY_EXE NAMES clang-tidy PATHS "$ENV{HOMEBREW_PREFIX}/opt/llvm/bin")
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

include(CTest)
# Enable testing in the root so CTest can find all tests
enable_testing()

add_subdirectory(libparsex)
add_subdirectory(parsex-cli)
add_subdirectory(parsex-mcp)
add_subdirectory(tests)

option(PARSEX_ENABLE_SANITIZERS "Build with ASan+UBSan instrumentation" OFF)

add_library(parsex_sanitizers INTERFACE)
if(PARSEX_ENABLE_SANITIZERS)
  target_compile_options(parsex_sanitizers INTERFACE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all)
  target_link_options(parsex_sanitizers INTERFACE
    -fsanitize=address,undefined)
endif()

target_link_libraries(libparsex PUBLIC parsex_sanitizers)