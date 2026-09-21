// XXE rejection tests — SECURITY-PURPOSED, do not fold into generic
// malformed-XML cases. These fixtures are classic external-entity
// proof-of-concept payloads (cf. OWASP's XXE Prevention Cheat Sheet),
// committed as attack specimens: the tests prove this specific attack does
// not work against this codebase. If the Loader's reader options ever change,
// these fail loudly in review instead of silently weakening.
//
// Current hardened behavior (XML_PARSE_NONET, no NOENT/DTDLOAD): libxml2
// refuses the unresolvable external entity ("Entity 'xxe' not defined") and
// the Loader surfaces ParseError[Syntax] — nothing resolved, nothing fetched,
// nothing on stderr. The tests accept EITHER a ParseError OR a parsed file
// with the entity left unexpanded, and pin the invariant both ways: attacker-
// controlled file content must never appear in the output.

#include <gtest/gtest.h>

#include <parsex/model/parsed_file.hpp>
#include <parsex/parser/loader.hpp>
#include <parsex/parser/parse_error.hpp>
#include <parsex/parser/parser.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

void collectTexts(const RawNode& node, std::string& out) {
    out += node.text;
    out += '\n';
    for (const auto& child : node.children) {
        collectTexts(*child, out);
    }
}

void expectNoDomainObjects(const ParsedFile& file) {
    EXPECT_TRUE(file.clusters.empty());
    EXPECT_TRUE(file.ecuInstances.empty());
    EXPECT_TRUE(file.frames.empty());
    EXPECT_TRUE(file.pdus.empty());
    EXPECT_TRUE(file.signals.empty());
    EXPECT_TRUE(file.signalGroups.empty());
}

// No attacker-controlled file content anywhere in the parsed output.
void expectNoResolvedContent(const ParsedFile& file, const std::string& marker) {
    expectNoDomainObjects(file);
    ASSERT_NE(file.rawDocument, nullptr);
    std::string texts;
    collectTexts(file.rawDocument->root, texts);
    EXPECT_EQ(texts.find(marker), std::string::npos) << "entity content leaked!";
}

// Either outcome is safe; anything else (crash, hang, resolved content,
// non-ParseError exception) fails the test.
void expectXxeRejected(const std::filesystem::path& path, const std::string& marker) {
    try {
        expectNoResolvedContent(Parser{}.parseFile(path), marker);
    } catch (const ParseError& err) {
        EXPECT_EQ(err.reason(), ParseErrorReason::Syntax);
    }
}

}  // namespace

TEST(XxeTest, LocalFileEntityRejected) {
    expectXxeRejected(fixture("xxe_attack.arxml"), "root:");
}

TEST(XxeTest, CanaryFileEntityRejected) {
#ifndef _WIN32
    // Stronger, platform-independent variant: reference a file WE created with
    // a known marker, so the test does not depend on /etc/passwd existing.
    // (POSIX-only: file:// URIs over temp paths are not portable to Windows;
    // the xxe_attack.arxml case above covers Windows by degrading to
    // unloadable-entity, which asserts the same way.)
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "parsex_xxe_canary_test";
    std::error_code ignored;
    std::filesystem::remove_all(scratch, ignored);
    std::filesystem::create_directories(scratch);
    const std::string marker = "PARSEX_XXE_CANARY_9f8b";
    {
        std::ofstream canary(scratch / "canary.txt", std::ios::binary);
        canary << marker << "\n";
    }
    const std::string payload =
        "<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE AUTOSAR [ <!ENTITY xxe SYSTEM \"file://" +
        scratch.string() +
        "/canary.txt\"> ]>\n"
        "<AUTOSAR>&xxe;</AUTOSAR>\n";
    const std::filesystem::path attack = scratch / "canary_attack.arxml";
    {
        std::ofstream out(attack, std::ios::binary);
        out << payload;
    }

    expectXxeRejected(attack, marker);

    std::filesystem::remove_all(scratch, ignored);
#else
    GTEST_SKIP() << "file:// canary URIs are POSIX-specific";
#endif
}

TEST(XxeTest, SsrfEntityRejectedWithoutFetch) {
    // If XML_PARSE_NONET were ever dropped AND substitution enabled, this
    // would hang on the unroutable metadata address until the global test
    // timeout — completing at all is the evidence the fetch never happens.
    expectXxeRejected(fixture("xxe_ssrf.arxml"), "ami-");
}
