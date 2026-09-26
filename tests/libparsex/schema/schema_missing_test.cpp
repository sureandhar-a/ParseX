// Broken backing setup: the registry's own files (not the input file) are
// missing or broken. Each must fail at init with a specific reason, mapped to
// IoErr/DataErr — never a confusing downstream failure or Software.
#include <gtest/gtest.h>

#include <parsex/schema/schema_registry.hpp>
#include <parsex/schema/schema_resolution_error.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

struct Guard {
    std::optional<std::string> xdg;
    std::optional<std::string> dir;
    Guard() {
        if (auto* v = std::getenv("XDG_CACHE_HOME")) xdg = v;
        if (auto* v = std::getenv("PARSEX_SCHEMA_DIR")) dir = v;
    }
    ~Guard() {
        if (xdg) setEnv("XDG_CACHE_HOME", *xdg);
        else unsetEnv("XDG_CACHE_HOME");
        if (dir) setEnv("PARSEX_SCHEMA_DIR", *dir);
        else unsetEnv("PARSEX_SCHEMA_DIR");
    }
};

std::filesystem::path freshRoot(const std::string& tag) {
    const auto root = std::filesystem::temp_directory_path() / ("parsex_missing_" + tag);
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    return root;
}

}  // namespace

TEST(SchemaMissingTest, NonexistentDirFailsSpecifically) {
    Guard guard;
    const auto root = freshRoot("nonexistent");
    setEnv("PARSEX_SCHEMA_DIR", (root / "nope").string());
    setEnv("XDG_CACHE_HOME", (root / "cache").string());
    // Unique release absent from every source (env dir, install default,
    // source tree) plus a fresh cache: must fail specifically, never generic.
    // Using 4.2.2 here would succeed via source-tree fallback, hiding the
    // missing-dir case.
    try {
        resolveSchema("9.9.9-missing-test");
        FAIL() << "should throw";
    } catch (const SchemaResolutionError& err) {
        EXPECT_EQ(err.reason(), SchemaResolutionReason::UnsupportedRelease);
    }
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(SchemaMissingTest, EmptyDirFailsSpecifically) {
    Guard guard;
    const auto root = freshRoot("empty");
    std::error_code ec;
    std::filesystem::create_directories(root / "sources", ec);
    setEnv("PARSEX_SCHEMA_DIR", (root / "sources").string());
    setEnv("XDG_CACHE_HOME", (root / "cache").string());
    try {
        resolveSchema("9.9.9-missing-test");
        FAIL() << "should throw";
    } catch (const SchemaResolutionError& err) {
        EXPECT_EQ(err.reason(), SchemaResolutionReason::UnsupportedRelease);
    }
    std::filesystem::remove_all(root, ec);
}

TEST(SchemaMissingTest, TruncatedFileFailsAsCorrupt) {
    Guard guard;
    const auto root = freshRoot("truncated");
    std::error_code ec;
    std::filesystem::create_directories(root / "sources", ec);
    {
        std::ofstream out(root / "sources" / "4.2.2.xsd", std::ios::binary | std::ios::trunc);
        out << "<?xml version=\"1.0\"?><xs:schema xmlns:xs=\"oops";
    }
    setEnv("PARSEX_SCHEMA_DIR", (root / "sources").string());
    setEnv("XDG_CACHE_HOME", (root / "cache").string());
    try {
        resolveSchema("4.2.2");
        FAIL() << "should throw";
    } catch (const SchemaResolutionError& err) {
        EXPECT_EQ(err.reason(), SchemaResolutionReason::SchemaFileCorrupt);
    }
    std::filesystem::remove_all(root, ec);
}

TEST(SchemaMissingTest, WrongContentFailsAsCorrupt) {
    Guard guard;
    const auto root = freshRoot("wrong");
    std::error_code ec;
    std::filesystem::create_directories(root / "sources", ec);
    {
        std::ofstream out(root / "sources" / "4.2.2.xsd", std::ios::binary | std::ios::trunc);
        out << "this is not an XSD at all, just text with the right filename";
    }
    setEnv("PARSEX_SCHEMA_DIR", (root / "sources").string());
    setEnv("XDG_CACHE_HOME", (root / "cache").string());
    try {
        resolveSchema("4.2.2");
        FAIL() << "should throw";
    } catch (const SchemaResolutionError& err) {
        EXPECT_EQ(err.reason(), SchemaResolutionReason::SchemaFileCorrupt);
    }
    std::filesystem::remove_all(root, ec);
}
