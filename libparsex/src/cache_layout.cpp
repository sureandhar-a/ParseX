#include <parsex/schema/cache_layout.hpp>

#include <cstdlib>
#include <system_error>

namespace {
std::filesystem::path& overridePath() {
    static std::filesystem::path path;
    return path;
}
bool& hasOverrideFlag() {
    static bool flag{false};
    return flag;
}
}  // namespace

void setCacheDirectoryOverride(const std::filesystem::path& dir) {
    overridePath() = dir;
    hasOverrideFlag() = true;
}

void clearCacheDirectoryOverride() {
    overridePath().clear();
    hasOverrideFlag() = false;
}

std::filesystem::path getCacheDirectory() {
    if (hasOverrideFlag()) {
        std::error_code dirError;
        std::filesystem::create_directories(overridePath(), dirError);
        return overridePath();
    }
    std::filesystem::path base;
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg != nullptr && *xdg != '\0') { // NOLINT(cppcoreguidelines-init-variables)
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') { // NOLINT(cppcoreguidelines-init-variables)
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
