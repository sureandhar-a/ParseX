// Facade tests (PAR-157): WriteEngine::write() wires tree construction +
// ordering + file write behind one entry point, threading the schema version
// from the ParsedProject (files[0].autosarRelease — already tracked by the
// Parser per file, so no new field was needed), and propagating inner-stage
// errors instead of letting libxml2 return codes go unchecked.

#include <gtest/gtest.h>

#include <parsex/model/parsed_file.hpp>
#include <parsex/model/parsed_project.hpp>
#include <parsex/write/write_engine.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

ParsedProject smallProject(const std::string& release) {
    ParsedProject project;
    ParsedFile file;
    file.autosarRelease = release;
    Cluster cluster;
    cluster.common.shortName = "C";
    cluster.baudrate = 500000U;
    file.clusters.push_back(cluster);
    project.files.push_back(file);
    return project;
}

}  // namespace

TEST(FacadeTest, NamespaceMatchesConfiguredSchemaVersion) {
    const auto path = std::filesystem::temp_directory_path() / "parsex_facade_a.arxml";
    WriteEngine{}.write(smallProject("4.4.0"), path);
    const std::string text = readFile(path);
    EXPECT_NE(text.find("xmlns=\"http://autosar.org/schema/r4.0\""), std::string::npos);
    EXPECT_NE(text.find("AUTOSAR_00046.xsd"), std::string::npos);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(FacadeTest, DifferentVersionsProduceDifferentNamespaces) {
    const auto pathA = std::filesystem::temp_directory_path() / "parsex_facade_b1.arxml";
    const auto pathB = std::filesystem::temp_directory_path() / "parsex_facade_b2.arxml";
    WriteEngine{}.write(smallProject("4.4.0"), pathA);
    WriteEngine{}.write(smallProject("R21-11"), pathB);
    const std::string textA = readFile(pathA);
    const std::string textB = readFile(pathB);
    EXPECT_NE(textA.find("http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd"), std::string::npos);
    EXPECT_NE(textB.find("http://autosar.org/schema/r21.11 AUTOSAR_00050.xsd"), std::string::npos);
    EXPECT_NE(textA, textB);
    std::error_code ignored;
    std::filesystem::remove(pathA, ignored);
    std::filesystem::remove(pathB, ignored);
}

TEST(FacadeTest, InnerStageErrorsPropagateAsStructuredThrows) {
    // Unwritable output path surfaces as a clear throw (not a crash, a
    // malformed file, or an unchecked libxml2 -1) and leaves no partial file.
    const auto bad = std::filesystem::path("/nonexistent_dir_xyz") / "parsex_facade_bad.arxml";
    EXPECT_THROW(WriteEngine{}.write(smallProject("4.4.0"), bad), std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(bad));
}
