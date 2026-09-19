// Cache layout tests: getCacheDirectory() honors $XDG_CACHE_HOME, falls back
// to $HOME/.cache, and cachePathFor() builds per-release file names.
// The tests only ever touch a temp directory — never the real user cache.

#include <gtest/gtest.h>
#include <parsex/schema/cache_layout.hpp>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace {

void setEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void unsetEnv(const char* name) {
#ifdef _WIN32
    // Empty is treated as unset by getCacheDirectory().
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

// Saves and restores XDG_CACHE_HOME/HOME around each test.
class EnvGuard {
public:
    EnvGuard() {
        xdg_ = readEnv("XDG_CACHE_HOME");
        home_ = readEnv("HOME");
    }
    ~EnvGuard() {
        restore("XDG_CACHE_HOME", xdg_);
        restore("HOME", home_);
    }
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;

private:
    static std::optional<std::string> readEnv(const char* name) {
        if (const char* value = std::getenv(name); value != nullptr) {
            return std::string(value);
        }
        return std::nullopt;
    }
    static void restore(const char* name, const std::optional<std::string>& saved) {
        if (saved.has_value()) {
            setEnv(name, *saved);
        } else {
            unsetEnv(name);
        }
    }

    std::optional<std::string> xdg_;
    std::optional<std::string> home_;
};

std::filesystem::path testScratchDir() {
    return std::filesystem::temp_directory_path() / "parsex_cache_layout_test";
}

}  // namespace

TEST(CacheLayoutTest, UsesXdgCacheHomeWhenSet) {
    const EnvGuard guard;
    const auto scratch = testScratchDir();
    std::error_code ec;
    std::filesystem::remove_all(scratch, ec);

    setEnv("XDG_CACHE_HOME", scratch.string());
    const auto dir = getCacheDirectory();

    EXPECT_EQ(dir, scratch / "parsex" / "schemas");
    EXPECT_TRUE(std::filesystem::is_directory(dir));

    std::filesystem::remove_all(scratch, ec);
}

TEST(CacheLayoutTest, FallsBackToHomeDotCache) {
    const EnvGuard guard;
    const auto fakeHome = testScratchDir() / "home";
    std::error_code ec;
    std::filesystem::remove_all(testScratchDir(), ec);

    unsetEnv("XDG_CACHE_HOME");
    setEnv("HOME", fakeHome.string());
    const auto dir = getCacheDirectory();

    EXPECT_EQ(dir, fakeHome / ".cache" / "parsex" / "schemas");
    EXPECT_TRUE(std::filesystem::is_directory(dir));

    std::filesystem::remove_all(testScratchDir(), ec);
}

TEST(CacheLayoutTest, CachePathForBuildsPerReleaseFileName) {
    const EnvGuard guard;
    const auto scratch = testScratchDir();
    std::error_code ec;
    std::filesystem::remove_all(scratch, ec);

    setEnv("XDG_CACHE_HOME", scratch.string());

    EXPECT_EQ(cachePathFor("4.2.2"), scratch / "parsex" / "schemas" / "4.2.2.xsd.cache");
    EXPECT_EQ(cachePathFor("R21-11"), scratch / "parsex" / "schemas" / "R21-11.xsd.cache");
    EXPECT_EQ(cachePathFor("R21-11").parent_path(), getCacheDirectory());

    std::filesystem::remove_all(scratch, ec);
}
