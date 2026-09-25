# Consuming the core library via the overlay port

`libparsex` ships as an overlay port in `ports/libparsex` (manifest plus
build file). It is not in the curated registry, so consumers reference this
repo's `ports/` directory directly. The port installs the library only —
never the command-line or server executables or their dependencies.

The consumer-side CMake usage below (`find_package` plus link) was tried end
to end against a scratch project and produces a working link. Installing the
port itself via `--overlay-ports` is covered by port validation.

## Option A: overlay port (vcpkg users)

1. Reference this repo (clone or submodule) somewhere stable, e.g.
   `third-party/ParseX`.
2. In the consumer's `vcpkg-configuration.json`, add the overlay:
   ```json
   {
       "overlay-ports": ["<path-to-ParseX>/ports"]
   }
   ```
   Or pass `--overlay-ports=<path-to-ParseX>/ports` on the vcpkg command
   line.
3. In the consumer's own `vcpkg.json`, add the dependency:
   ```json
   {
       "dependencies": ["libparsex"]
   }
   ```
4. In the consumer's `CMakeLists.txt` (needs C++20):
   ```cmake
   find_package(libparsex CONFIG REQUIRED)
   target_link_libraries(myapp PRIVATE libparsex::libparsex)
   ```
   The exported target is `libparsex::libparsex` — exactly what the port's
   package config provides.

## Option B: submodule plus add_subdirectory (no vcpkg)

For teams not using vcpkg at all:

```bash
git submodule add https://github.com/sureandhar-a/ParseX.git third-party/ParseX
```

```cmake
add_subdirectory(third-party/ParseX)
target_link_libraries(myapp PRIVATE libparsex)
```

This builds the library from source with the consumer's own toolchain and
skips the port entirely. Disable unneeded components to keep the build lean:
`-DPARSEX_BUILD_CLI=OFF -DPARSEX_BUILD_MCP=OFF -DPARSEX_BUILD_TESTS=OFF`.
