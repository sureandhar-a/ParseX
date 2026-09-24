#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "CLI/CLI.hpp"

namespace fs = std::filesystem;

// PAR-222: lock in allow_config_extras() decision — default is Ignore (unknown
// keys silently ignored). Also documents the misspelled-key sharp edge.

namespace {

fs::path tempConfig(const std::string& content) {
    fs::path p = fs::temp_directory_path() / ("parsex_config_test_" + std::to_string(::getpid()) + ".toml");
    // Use a unique per-test suffix to avoid collision when tests run in parallel.
    static int counter = 0;
    p = fs::path(p.string() + std::to_string(counter++));
    std::ofstream out(p);
    out << content;
    return p;
}

}  // namespace

TEST(ConfigExtras, UnknownKeyIsIgnored) {
    CLI::App app{"test"};
    std::string schemaCacheDir;
    app.add_option("--schema-cache-dir", schemaCacheDir, "cache dir");
    app.set_config("--config", "", "Read a config file", false);
    // Not calling allow_config_extras -> default Ignore

    fs::path cfg = tempConfig("schema-cache-dir=\"/tmp/from-config\"\nunknown-key=\"oops\"\n");
    const char* argv[] = {"prog", "--config", cfg.c_str()};
    ASSERT_NO_THROW(app.parse(3, const_cast<char**>(argv)));
    EXPECT_EQ(schemaCacheDir, "/tmp/from-config");
    // Unknown key did not cause failure — documented Ignore behavior.
    fs::remove(cfg);
}

TEST(ConfigExtras, MisspelledKeySilentlyIgnoredSharpEdge) {
    CLI::App app{"test"};
    std::string schemaCacheDir;
    app.add_option("--schema-cache-dir", schemaCacheDir, "cache dir");
    app.set_config("--config", "", "Read a config file", false);

    // Intentional typo: schema-cach-dir (missing 'e')
    fs::path cfg = tempConfig("schema-cach-dir=\"/tmp/typo-path\"\n");
    const char* argv[] = {"prog", "--config", cfg.c_str()};
    ASSERT_NO_THROW(app.parse(3, const_cast<char**>(argv)));
    // Typo is silently ignored — no error, value stays empty. This is the
    // known sharp edge when allow_config_extras is Ignore: CLI11 cannot warn
    // about a misspelled real key without a custom validator. Documented in
    // parsex-cli/main.cpp next to set_config().
    EXPECT_TRUE(schemaCacheDir.empty());
    fs::remove(cfg);
}
