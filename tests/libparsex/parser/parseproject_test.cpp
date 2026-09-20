// parseProject() with ExplicitList: each named file parses independently via
// parseFile() and lands in ParsedProject.files, in order. No cross-file
// reference resolution yet (later subtask) — resolvedRefs stays empty here.

#include <gtest/gtest.h>

#include <parsex/model/parsed_project.hpp>
#include <parsex/parser/parser.hpp>
#include <parsex/parser/project_error.hpp>
#include <parsex/parser/release_error.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

std::string readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void writeBytes(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary);
    output << bytes;
}

std::vector<std::string> parsedBasenames(const ParsedProject& project) {
    std::vector<std::string> names;
    names.reserve(project.files.size());
    for (const auto& file : project.files) {
        names.push_back(file.sourcePath.filename().string());
    }
    std::ranges::sort(names);
    return names;
}

std::filesystem::path freshScratch(const std::string& name) {
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / name;
    std::error_code ignored;
    std::filesystem::remove_all(scratch, ignored);
    std::filesystem::create_directories(scratch);
    return scratch;
}

std::string arxmlDoc(const std::string& body) {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<AUTOSAR xmlns=\"http://autosar.org/schema/r4.0\" "
           "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
           "xsi:schemaLocation=\"http://autosar.org/schema/r4.0 AUTOSAR_00046.xsd\">\n" +
           body + "</AUTOSAR>\n";
}

const ParsedFile* findFile(const ParsedProject& project, const std::string& name) {
    for (const auto& file : project.files) {
        if (file.sourcePath.filename() == name) {
            return &file;
        }
    }
    return nullptr;
}

}  // namespace

TEST(ParseProjectTest, ExplicitListCollectsThreeFilesInOrder) {
    const std::vector<std::filesystem::path> entries = {
        fixture("parsefile_complete.arxml"),
        fixture("system-4.2.arxml"),
        fixture("release_multiline.arxml"),
    };
    const ParsedProject project =
        Parser{}.parseProject(entries, FileDiscoveryMode::ExplicitList);

    ASSERT_EQ(project.files.size(), 3U);
    EXPECT_EQ(project.files.at(0).autosarRelease, "4.4.0");
    EXPECT_EQ(project.files.at(0).sourcePath, entries.at(0));
    EXPECT_EQ(project.files.at(0).frames.size(), 1U);
    EXPECT_EQ(project.files.at(1).autosarRelease, "4.4.0");
    EXPECT_EQ(project.files.at(1).sourcePath, entries.at(1));
    EXPECT_EQ(project.files.at(1).frames.size(), 8U);
    EXPECT_EQ(project.files.at(2).autosarRelease, "4.2.2");
    EXPECT_EQ(project.files.at(2).sourcePath, entries.at(2));

    // No cross-file resolution yet.
    EXPECT_TRUE(project.resolvedRefs.empty());
}

TEST(ParseProjectTest, OneFailingFileFailsTheWholeCall) {
    // tiny_valid.arxml declares out-of-range autosar_4_0_0.xsd: its
    // UnsupportedReleaseError must escape instead of yielding partial results.
    const std::vector<std::filesystem::path> entries = {
        fixture("parsefile_complete.arxml"),
        fixture("tiny_valid.arxml"),
    };
    EXPECT_THROW(Parser{}.parseProject(entries, FileDiscoveryMode::ExplicitList),
                 UnsupportedReleaseError);
}

TEST(ParseProjectTest, DirectoryScanFindsSiblingsOnly) {
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "parsex_dirscan_test";
    std::error_code ignored;
    std::filesystem::remove_all(scratch, ignored);
    std::filesystem::create_directories(scratch / "sub");

    // One entry point, two siblings (one uppercase extension), one unrelated
    // file, and one nested file the single-level scan must not reach.
    const std::string body = readBytes(fixture("parsefile_complete.arxml"));
    writeBytes(scratch / "entry.arxml", body);
    writeBytes(scratch / "sibling.arxml", body);
    writeBytes(scratch / "upper.ARXML", body);
    writeBytes(scratch / "notes.txt", "not xml\n");
    writeBytes(scratch / "sub" / "nested.arxml", body);

    const ParsedProject project = Parser{}.parseProject(
        {scratch / "entry.arxml"}, FileDiscoveryMode::DirectoryScan);
    EXPECT_EQ(parsedBasenames(project),
              std::vector<std::string>({"entry.arxml", "sibling.arxml", "upper.ARXML"}));
    for (const auto& file : project.files) {
        EXPECT_EQ(file.autosarRelease, "4.4.0");
    }

    // A second entry point in the same directory adds no duplicates.
    const ParsedProject deduped = Parser{}.parseProject(
        {scratch / "entry.arxml", scratch / "sibling.arxml"},
        FileDiscoveryMode::DirectoryScan);
    EXPECT_EQ(deduped.files.size(), 3U);

    std::filesystem::remove_all(scratch, ignored);
}

TEST(ParseProjectTest, LazyDiscoversTwoLevelsDeep) {
    const std::filesystem::path scratch = freshScratch("parsex_lazy_test");
    std::error_code ignored;
    std::filesystem::create_directories(scratch / "sub" / "deep");

    // Entry references a PDU, an ECU, and a signal member — none defined
    // locally. The recursive search must find them two levels down, where the
    // single-level DirectoryScan cannot reach.
    writeBytes(scratch / "entry.arxml",
               arxmlDoc("  <FRAME><SHORT-NAME>F0</SHORT-NAME><LENGTH>8</LENGTH>"
                        "<TRANSMITTERS><TRANSMITTER-REF>/E/E9</TRANSMITTER-REF>"
                        "</TRANSMITTERS><PDUS><FRAME-PDU><PDU-REF>/P/P1</PDU-REF>"
                        "<START-POSITION>0</START-POSITION></FRAME-PDU></PDUS></FRAME>"
                        "<SIGNAL-GROUP><SHORT-NAME>G0</SHORT-NAME><MEMBERS>"
                        "<SYSTEM-SIGNAL-REF>/S/S2</SYSTEM-SIGNAL-REF>"
                        "</MEMBERS></SIGNAL-GROUP>"));
    writeBytes(scratch / "sub" / "deep" / "second.arxml",
               arxmlDoc("  <ECU-INSTANCE><SHORT-NAME>E9</SHORT-NAME></ECU-INSTANCE>"
                        "  <PDU><SHORT-NAME>P1</SHORT-NAME><LENGTH>8</LENGTH></PDU>"
                        "  <SYSTEM-SIGNAL><SHORT-NAME>S2</SHORT-NAME>"
                        "<BIT-LENGTH>4</BIT-LENGTH></SYSTEM-SIGNAL>"));

    const ParsedProject project = Parser{}.parseProject(
        {scratch / "entry.arxml"}, FileDiscoveryMode::LazyOnReference);
    ASSERT_EQ(project.files.size(), 2U);
    EXPECT_EQ(parsedBasenames(project),
              std::vector<std::string>({"entry.arxml", "second.arxml"}));

    const ParsedFile* entry = findFile(project, "entry.arxml");
    const ParsedFile* second = findFile(project, "second.arxml");
    ASSERT_NE(entry, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_EQ(entry->frames.size(), 1U);
    EXPECT_EQ(entry->frames.at(0).transmitters, std::vector<std::string>{"E9"});
    EXPECT_EQ(entry->frames.at(0).pdus.at(0).pduShortNameRef, "P1");
    ASSERT_EQ(second->pdus.size(), 1U);
    EXPECT_EQ(second->pdus.at(0).common.shortName, "P1");
    EXPECT_EQ(second->pdus.at(0).length, 8U);
    ASSERT_EQ(entry->signalGroups.size(), 1U);
    EXPECT_EQ(entry->signalGroups.at(0).members, std::vector<std::string>{"S2"});

    // Contrast: DirectoryScan sees only the top level here.
    const ParsedProject flat = Parser{}.parseProject(
        {scratch / "entry.arxml"}, FileDiscoveryMode::DirectoryScan);
    EXPECT_EQ(flat.files.size(), 1U);

    std::filesystem::remove_all(scratch, ignored);
}

TEST(ParseProjectTest, LazyReferenceCycleTerminates) {
    const std::filesystem::path scratch = freshScratch("parsex_lazy_cycle_test");
    std::error_code ignored;

    // A -> B -> A: entry wants P_B, fileB defines P_B but wants S_A, which the
    // entry already defines. No false cycle alarm — two files, then done.
    writeBytes(scratch / "entry.arxml",
               arxmlDoc("  <FRAME><SHORT-NAME>F</SHORT-NAME><LENGTH>8</LENGTH>"
                        "<PDUS><FRAME-PDU><PDU-REF>/P/P_B</PDU-REF>"
                        "<START-POSITION>0</START-POSITION></FRAME-PDU></PDUS></FRAME>"
                        "  <SYSTEM-SIGNAL><SHORT-NAME>S_A</SHORT-NAME>"
                        "<BIT-LENGTH>1</BIT-LENGTH></SYSTEM-SIGNAL>"));
    writeBytes(scratch / "fileB.arxml",
               arxmlDoc("  <PDU><SHORT-NAME>P_B</SHORT-NAME><LENGTH>8</LENGTH>"
                        "<SIGNAL-MAPPINGS><PDU-SIGNAL-MAPPING>"
                        "<SIGNAL-REF>/S/S_A</SIGNAL-REF>"
                        "<START-POSITION>0</START-POSITION>"
                        "</PDU-SIGNAL-MAPPING></SIGNAL-MAPPINGS></PDU>"));

    const ParsedProject project = Parser{}.parseProject(
        {scratch / "entry.arxml"}, FileDiscoveryMode::LazyOnReference);
    EXPECT_EQ(project.files.size(), 2U);

    std::filesystem::remove_all(scratch, ignored);
}

TEST(ParseProjectTest, LazyUnresolvableRefFailsFast) {
    const std::filesystem::path scratch = freshScratch("parsex_lazy_dangling_test");
    std::error_code ignored;

    writeBytes(scratch / "entry.arxml",
               arxmlDoc("  <FRAME><SHORT-NAME>F</SHORT-NAME><LENGTH>8</LENGTH>"
                        "<PDUS><FRAME-PDU><PDU-REF>/P/P_GHOST</PDU-REF>"
                        "<START-POSITION>0</START-POSITION></FRAME-PDU></PDUS></FRAME>"));

    try {
        Parser{}.parseProject({scratch / "entry.arxml"},
                              FileDiscoveryMode::LazyOnReference);
        FAIL() << "expected DanglingFileReferenceError";
    } catch (const DanglingFileReferenceError& err) {
        EXPECT_NE(std::find(err.refs().begin(), err.refs().end(), "P_GHOST"),
                  err.refs().end());
        EXPECT_NE(std::string(err.what()).find("P_GHOST"), std::string::npos);
    }

    std::filesystem::remove_all(scratch, ignored);
}

TEST(ParseProjectTest, LazyIterationCapStopsLongChain) {
    const std::filesystem::path scratch = freshScratch("parsex_lazy_chain_test");
    std::error_code ignored;

    // 56 files: file_k defines P_k and (for k >= 1) S_k; P_k maps S_{k+1}.
    // Every iteration adds exactly one file, so the 50-iteration cap must
    // fire with S_51 still missing — completing (not hanging) is the proof.
    const int fileCount = 56;
    for (int idx = 0; idx < fileCount; ++idx) {
        std::string body = "  <PDU><SHORT-NAME>P_" + std::to_string(idx) +
                           "</SHORT-NAME><LENGTH>8</LENGTH><SIGNAL-MAPPINGS>"
                           "<PDU-SIGNAL-MAPPING><SIGNAL-REF>/S/S_" +
                           std::to_string(idx + 1) +
                           "</SIGNAL-REF><START-POSITION>0</START-POSITION>"
                           "</PDU-SIGNAL-MAPPING></SIGNAL-MAPPINGS></PDU>";
        if (idx > 0) {
            body += "  <SYSTEM-SIGNAL><SHORT-NAME>S_" + std::to_string(idx) +
                    "</SHORT-NAME><BIT-LENGTH>8</BIT-LENGTH></SYSTEM-SIGNAL>";
        }
        writeBytes(scratch / ("file_" + std::to_string(idx) + ".arxml"), arxmlDoc(body));
    }

    try {
        Parser{}.parseProject({scratch / "file_0.arxml"},
                              FileDiscoveryMode::LazyOnReference);
        FAIL() << "expected DanglingFileReferenceError from the iteration cap";
    } catch (const DanglingFileReferenceError& err) {
        EXPECT_NE(std::find(err.refs().begin(), err.refs().end(), "S_51"),
                  err.refs().end());
    }

    std::filesystem::remove_all(scratch, ignored);
}
