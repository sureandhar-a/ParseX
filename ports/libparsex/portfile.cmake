# Overlay port for the ParseX core library. Installs libparsex only — the
# command-line and server executables are not part of this port, so a
# consumer never pulls their dependencies.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sureandhar-a/ParseX
    REF "v${VERSION}"
    # PINNED ON RELEASE: replace with the real SHA512 of the vX.Y.Z source
    # archive when the release tag is cut (the pre-release checklist covers
    # this). The tag does not exist until the first release, so no real hash
    # can be recorded yet.
    SHA512 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
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
