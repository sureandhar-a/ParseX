# Define the static library target and add source files
add_library(libparsex STATIC
    src/version.cpp
    src/cache_layout.cpp
    src/atomic_write.cpp
    src/schema_registry.cpp
    src/secure_reader.cpp
    src/loader.cpp
    src/release_detector.cpp
    src/model_builder.cpp
    src/parser.cpp
    src/json_contract/envelope.cpp
    src/json_contract/schema_validate.cpp
    src/telemetry/span_status.cpp
    src/telemetry/span_id.cpp
    src/telemetry/trace.cpp
    src/telemetry/telemetry_config.cpp
    src/telemetry/telemetry_context.cpp
    src/telemetry/scoped_span.cpp
    src/diff/diff_report.cpp
    src/diff/diff_match.cpp
    src/diff/diff_moves.cpp
    src/diff/diff_populate.cpp
    src/diff/diff_render.cpp
    src/diff/diff_engine.cpp
    src/validator/validation_result.cpp
    src/validator/validator_schema.cpp
    src/validator/validator_references.cpp
    src/validator/validator_uniqueness.cpp
    src/validator/validator_can.cpp
    src/validator/validator_facade.cpp
    src/write/write_context.cpp
    src/write/write_elements.cpp
    src/write/write_ordering.cpp
    src/write/write_format.cpp
    src/write/write_engine.cpp
)

# Tell targets linking against libparsex where to find its header files.
# Generator expressions keep build-tree paths out of the installed export so
# `cmake --install` (and the packaging port) works.
target_include_directories(libparsex PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

# The library's own sources also include <parsex/...>.
target_include_directories(libparsex PRIVATE include)

# Baked-in schema source fallbacks for resolveSchema(): the install default
# first, then the source tree (dev convenience). $PARSEX_SCHEMA_DIR, checked
# at runtime, outranks both.
target_compile_definitions(libparsex PRIVATE
    PARSEX_DEFAULT_SCHEMA_DIR="${CMAKE_INSTALL_PREFIX}/share/parsex/schemas"
    PARSEX_SOURCE_SCHEMA_DIR="${CMAKE_SOURCE_DIR}/resources/schemas"
)


find_package(LibXml2 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(nlohmann_json_schema_validator CONFIG REQUIRED)
find_package(boost_pfr CONFIG REQUIRED)

# Link dependencies to the library target so they propagate to consumers
target_link_libraries(libparsex PUBLIC LibXml2::LibXml2 nlohmann_json::nlohmann_json
                                       nlohmann_json_schema_validator::validator
                                       Boost::pfr)

# Install + CMake package config so downstream projects can
# `find_package(libparsex CONFIG REQUIRED)` and link `libparsex::libparsex`.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS libparsex parsex_sanitizers parsex_coverage EXPORT libparsexTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "${CMAKE_BINARY_DIR}/generated/parsex/version_config.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/parsex")

install(EXPORT libparsexTargets
        FILE libparsexTargets.cmake
        NAMESPACE libparsex::
        DESTINATION "share/libparsex")

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/libparsexConfig.cmake.in"
                              "${CMAKE_BINARY_DIR}/libparsexConfig.cmake"
                              INSTALL_DESTINATION "share/libparsex")
write_basic_package_version_file("${CMAKE_BINARY_DIR}/libparsexConfigVersion.cmake"
                                 VERSION "${PROJECT_VERSION}"
                                 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_BINARY_DIR}/libparsexConfig.cmake"
              "${CMAKE_BINARY_DIR}/libparsexConfigVersion.cmake"
        DESTINATION "share/libparsex")
