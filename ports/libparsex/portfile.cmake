# Overlay port for the ParseX core library. Installs libparsex only — the
# command-line and server executables are not part of this port, so a
# consumer never pulls their dependencies.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sureandhar-a/ParseX
    REF "v${VERSION}"
    # Pinned to the v0.2.0 source archive; re-pin on every release (see
    # docs/release-checklist.md "Port hash pinning").
    SHA512 398744336abd283594145d6fef112c814c5dc7a0e169ff1a6226ce846a505dcb52bb37c60e70eefbf3a784ce3113913d0d033413ea10cee160de4b8f927e631e
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DPARSEX_BUILD_CLI=OFF
        -DPARSEX_BUILD_MCP=OFF
        -DPARSEX_BUILD_TESTS=OFF
        -DPARSEX_ENABLE_FUZZING=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH share/libparsex)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
