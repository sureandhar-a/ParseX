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
)

# Tell targets linking against libparsex where to find its header files
target_include_directories(libparsex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(libparsex INTERFACE include)

# The library's own sources also include <parsex/...> (INTERFACE above only
# applies to consumers).
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
find_package(boost_pfr CONFIG REQUIRED)

# Link dependencies to the library target so they propagate to consumers
target_link_libraries(libparsex PUBLIC LibXml2::LibXml2 nlohmann_json::nlohmann_json
                                       Boost::pfr)
