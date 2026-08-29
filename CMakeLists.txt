cmake_minimum_required(VERSION 3.21)

# 1. READ VCPKG_ROOT FROM YOUR SYSTEM ENVIRONMENT
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(ParseX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS)

add_subdirectory(libparsex)
add_subdirectory(parsex-cli)
add_subdirectory(parsex-mcp)
add_subdirectory(tests)