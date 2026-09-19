# Define the static library target and add source files
add_library(libparsex STATIC
    src/version.cpp
    src/cache_layout.cpp
    src/atomic_write.cpp
)

# Tell targets linking against libparsex where to find its header files
target_include_directories(libparsex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(libparsex INTERFACE include)

# The library's own sources also include <parsex/...> (INTERFACE above only
# applies to consumers).
target_include_directories(libparsex PRIVATE include)


find_package(LibXml2 REQUIRED)
find_package(nlohmann_json REQUIRED)

# Link dependencies to the library target so they propagate to consumers
target_link_libraries(libparsex PUBLIC LibXml2::LibXml2 nlohmann_json::nlohmann_json)
