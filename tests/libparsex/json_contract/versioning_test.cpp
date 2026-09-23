// Presence/structure check for schemas/VERSIONING.md (PAR-171).
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

TEST(VersioningDocTest, ClassificationTableExistsWithAllRows) {
#ifdef PARSEX_VERSIONING_FILE
    const std::string path = PARSEX_VERSIONING_FILE;
#else
    const std::string path = "schemas/VERSIONING.md";
#endif
    ASSERT_TRUE(fs::exists(path)) << "missing versioning doc: " << path;
    std::ifstream in(path);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // The 6 classification rows from PAR-171 step 3 (key phrases).
    EXPECT_NE(text.find("Adding an optional property"), std::string::npos);
    EXPECT_NE(text.find("Removing an optional property"), std::string::npos);
    EXPECT_NE(text.find("Adding a required property"), std::string::npos);
    EXPECT_NE(text.find("Removing a required property"), std::string::npos);
    EXPECT_NE(text.find("required/optional status"), std::string::npos);
    EXPECT_NE(text.find("Removing a value from an enumeration"), std::string::npos);
    EXPECT_NE(text.find("Changing a field's type"), std::string::npos);

    // Sourcing honesty markers.
    EXPECT_NE(text.find("Creek Service"), std::string::npos);
    EXPECT_NE(text.find("Azure"), std::string::npos);
    EXPECT_NE(text.find("ParseX-specific extension"), std::string::npos);
    EXPECT_NE(text.find("Confluent"), std::string::npos);
}
