// SchemaResolutionError tests: nonsense release -> UnsupportedRelease,
// corrupted .xsd -> SchemaFileCorrupt (both asserting reason code and that
// the message names the release), plus CacheWriteFailed degrading gracefully
// to parsing the source directly. Hermetic temp dirs via env vars, as usual.

#include <gtest/gtest.h>
#include <parsex/schema/schema_registry.hpp>
#include <parsex/schema/schema_resolution_error.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
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
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

class EnvGuard {
public:
    EnvGuard() {
        xdg_ = readEnv("XDG_CACHE_HOME");
        dir_ = readEnv("PARSEX_SCHEMA_DIR");
    }
    ~EnvGuard() {
        restore("XDG_CACHE_HOME", xdg_);
        restore("PARSEX_SCHEMA_DIR", dir_);
    }
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;
    EnvGuard(EnvGuard&&) noexcept = default;
    EnvGuard& operator=(EnvGuard&&) noexcept = default;

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
    std::optional<std::string> dir_;
};

struct TestDirs {
    std::filesystem::path root;
    std::filesystem::path sources;
    std::filesystem::path cacheHome;
};

// Fresh temp dirs on both env vars. Callers hold an EnvGuard declared first.
TestDirs makeIsolatedDirs() {
    TestDirs dirs;
    dirs.root = std::filesystem::temp_directory_path() / "parsex_schema_error_test";
    dirs.sources = dirs.root / "sources";
    dirs.cacheHome = dirs.root / "cache-home";
    std::error_code errCode;
    std::filesystem::remove_all(dirs.root, errCode);
    setEnv("PARSEX_SCHEMA_DIR", dirs.sources.string());
    setEnv("XDG_CACHE_HOME", dirs.cacheHome.string());
    std::filesystem::create_directories(dirs.sources, errCode);
    return dirs;
}

void seedSource(const TestDirs& dirs, const std::string& release, const std::string& fixture) {
    std::filesystem::copy_file(
        std::filesystem::path(PARSEX_FIXTURE_DIR) / fixture,
        dirs.sources / (release + ".xsd"));
}

void clearScratch(const TestDirs& dirs) {
    std::error_code errCode;
    std::filesystem::remove_all(dirs.root, errCode);
}

// Calls resolveSchema() expecting a SchemaResolutionError and returns a copy
// for the caller to inspect. Fails the test when nothing (or anything else)
// is thrown.
SchemaResolutionError catchResolutionError(const std::string& release) {
    try {
        resolveSchema(release);
    } catch (const SchemaResolutionError& err) {
        return err;
    } catch (...) {
        ADD_FAILURE() << "resolveSchema(\"" << release << "\") threw a non-SchemaResolutionError";
        throw std::logic_error("wrong exception type");
    }
    ADD_FAILURE() << "resolveSchema(\"" << release << "\") did not throw";
    throw std::logic_error("no exception thrown");
}

void expectReasonAndRelease(
    const SchemaResolutionError& err,
    SchemaResolutionReason reason,
    const std::string& release) {
    EXPECT_EQ(err.reason(), reason);
    EXPECT_EQ(err.release(), release);
    EXPECT_NE(std::string(err.what()).find(release), std::string::npos)
        << "message names the release: " << err.what();
}

}  // namespace

TEST(SchemaResolutionErrorTest, NonsenseReleaseIsUnsupported) {
    EnvGuard guard;
    const TestDirs dirs = makeIsolatedDirs();

    expectReasonAndRelease(
        catchResolutionError("nope-9.9"), SchemaResolutionReason::UnsupportedRelease, "nope-9.9");

    clearScratch(dirs);
}

TEST(SchemaResolutionErrorTest, CorruptedXsdIsSchemaFileCorrupt) {
    EnvGuard guard;
    const TestDirs dirs = makeIsolatedDirs();
    seedSource(dirs, "corrupt-test", "corrupt_test_schema.xsd");

    expectReasonAndRelease(
        catchResolutionError("corrupt-test"),
        SchemaResolutionReason::SchemaFileCorrupt,
        "corrupt-test");

    clearScratch(dirs);
}

TEST(SchemaResolutionErrorTest, CacheWriteFailureDegradesGracefully) {
    EnvGuard guard;
    const TestDirs dirs = makeIsolatedDirs();
    seedSource(dirs, "mini-test", "mini_test_schema.xsd");

    // Sabotage the cache entry: a non-empty directory where the cache file
    // would go makes rename() fail, so writeCacheAtomically() throws.
    const auto blocked =
        dirs.cacheHome / "parsex" / "schemas" / "mini-test.xsd.cache";
    std::error_code errCode;
    std::filesystem::create_directories(blocked, errCode);
    std::ofstream(blocked / "blocker", std::ios::binary) << "occupied";

    // Still resolves — by parsing the source directly instead of failing.
    SchemaResolutionResult result;
    EXPECT_NO_THROW(result = resolveSchema("mini-test"));
    EXPECT_FALSE(result.wasCacheHit);
    EXPECT_EQ(result.schema.autosarRelease, "mini-test");
    EXPECT_NE(result.schema.schemaHandle, nullptr);
    EXPECT_EQ(result.schema.sourceXsdPath, dirs.sources / "mini-test.xsd");

    clearScratch(dirs);
}
