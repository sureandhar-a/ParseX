// resolveSchema() tests: cache miss then hit for the same release, telemetry
// timing on/off, and unsupported-release failure. Fully hermetic: a temp
// source dir (seeded from a tiny fixture XSD) plus a temp cache dir, both
// pointed at via environment variables.

#include <gtest/gtest.h>
#include <parsex/schema/schema_registry.hpp>
#include <parsex/schema/schema_resolution_error.hpp>
#include <parsex/telemetry/operation_telemetry.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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
    std::filesystem::path sources;
    std::filesystem::path cacheHome;
};

// Points XDG_CACHE_HOME and PARSEX_SCHEMA_DIR at fresh temp dirs and seeds
// the source dir with the fixture as "<release>.xsd". Callers must hold an
// EnvGuard declared BEFORE this call so env is restored afterwards.
TestDirs makeIsolatedDirs(const std::string& release) {
    const auto root = std::filesystem::temp_directory_path() / "parsex_resolve_schema_test";
    std::error_code errCode;
    std::filesystem::remove_all(root, errCode);

    TestDirs dirs{.sources = root / "sources", .cacheHome = root / "cache-home"};
    setEnv("PARSEX_SCHEMA_DIR", dirs.sources.string());
    setEnv("XDG_CACHE_HOME", dirs.cacheHome.string());

    std::filesystem::create_directories(dirs.sources, errCode);
    std::filesystem::copy_file(
        std::filesystem::path(PARSEX_FIXTURE_DIR) / "mini_test_schema.xsd",
        dirs.sources / (release + ".xsd"));
    return dirs;
}

std::string readFileBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(ResolveSchemaTest, FirstCallIsCacheMissSecondIsHit) {
    EnvGuard guard;
    TestDirs dirs = makeIsolatedDirs("mini-test");

    const SchemaResolutionResult miss = resolveSchema("mini-test");
    EXPECT_FALSE(miss.wasCacheHit);
    EXPECT_EQ(miss.schema.autosarRelease, "mini-test");
    EXPECT_NE(miss.schema.schemaHandle, nullptr);
    EXPECT_EQ(miss.schema.sourceXsdPath, dirs.sources / "mini-test.xsd");

    // The miss populated the cache with identical bytes.
    const auto cached = dirs.cacheHome / "parsex" / "schemas" / "mini-test.xsd.cache";
    ASSERT_TRUE(std::filesystem::is_regular_file(cached));
    EXPECT_EQ(readFileBytes(cached), readFileBytes(dirs.sources / "mini-test.xsd"));

    const SchemaResolutionResult hit = resolveSchema("mini-test");
    EXPECT_TRUE(hit.wasCacheHit);
    EXPECT_EQ(hit.schema.autosarRelease, "mini-test");
    EXPECT_NE(hit.schema.schemaHandle, nullptr);
    EXPECT_EQ(hit.schema.sourceXsdPath, cached);

    std::error_code errCode;
    std::filesystem::remove_all(dirs.sources.parent_path(), errCode);
}

TEST(ResolveSchemaTest, TelemetryTimedWhenNonNullFreeWhenNull) {
    EnvGuard guard;
    TestDirs dirs = makeIsolatedDirs("mini-test");

    OperationTelemetry telemetry;
    const SchemaResolutionResult result = resolveSchema("mini-test", &telemetry);
    EXPECT_NE(result.schema.schemaHandle, nullptr);
    ASSERT_EQ(telemetry.samples.size(), 1U);
    EXPECT_EQ(telemetry.samples.at(0).name, "SchemaRegistry.resolveSchema");
    EXPECT_GE(telemetry.samples.at(0).elapsed.count(), 0);

    // Nullptr costs nothing and still resolves.
    const SchemaResolutionResult plain = resolveSchema("mini-test", nullptr);
    EXPECT_TRUE(plain.wasCacheHit);
    EXPECT_NE(plain.schema.schemaHandle, nullptr);

    std::error_code errCode;
    std::filesystem::remove_all(dirs.sources.parent_path(), errCode);
}

TEST(ResolveSchemaTest, UnsupportedReleaseThrows) {
    EnvGuard guard;
    TestDirs dirs = makeIsolatedDirs("mini-test");

    EXPECT_THROW(resolveSchema("no-such-release"), SchemaResolutionError);

    std::error_code errCode;
    std::filesystem::remove_all(dirs.sources.parent_path(), errCode);
}
