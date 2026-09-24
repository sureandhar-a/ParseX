#include <parsex/schema/cache_layout.hpp>

#include <cstdlib>
#include <system_error>

namespace {
std::filesystem::path g_override;
bool g_hasOverride{false};
}  // namespace

void setCacheDirectoryOverride(const std::filesystem::path& dir) {
    g_override = dir;
    g_hasOverride = true;
}

void clearCacheDirectoryOverride() {
    g_override.clear();
    g_hasOverride = false;
}

std::filesystem::path getCacheDirectory() {
    if (g_hasOverride) {
        std::error_code dirError;
        std::filesystem::create_directories(g_override, dirError);
        return g_override;
    }
    std::filesystem::path base;
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg != nullptr && *xdg != '\0') {
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        base = std::filesystem::path(home) / ".cache";
    } else {
        base = std::filesystem::temp_directory_path();
    }

    const std::filesystem::path dir = base / "parsex" / "schemas";
    std::error_code dirError;
    std::filesystem::create_directories(dir, dirError);
    return dir;
}

std::filesystem::path cachePathFor(const std::string& release) {
    return getCacheDirectory() / (release + ".xsd.cache");
}
