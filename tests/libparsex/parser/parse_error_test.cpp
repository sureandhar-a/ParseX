// ParseError paths: I/O (missing/unreadable file) and Syntax (malformed XML)
// must surface as ParseError with the right reason code and genuinely
// informative messages — path for I/O, line/column plus libxml2's own text
// for syntax — never a bare "parse failed".

#include <gtest/gtest.h>

#include <parsex/parser/loader.hpp>
#include <parsex/parser/parse_error.hpp>
#include <parsex/parser/parser.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace {

std::filesystem::path fixture(const std::string& name) {
    return std::filesystem::path(PARSEX_FIXTURE_DIR) / name;
}

template <typename Action>
std::optional<ParseError> thrownBy(const Action& action) {
    try {
        action();
    } catch (const ParseError& err) {
        return err;
    }
    return std::nullopt;
}

void expectContains(const std::string& message, const std::string& needle) {
    EXPECT_NE(message.find(needle), std::string::npos) << "missing: " << needle;
}

}  // namespace

TEST(ParseErrorTest, MissingFileReportsIoWithPath) {
    const auto path = fixture("does-not-exist.arxml");
    const std::optional<ParseError> err =
        thrownBy([&] { loadRawDocument(path); });
    ASSERT_TRUE(err.has_value());
    if (err.has_value()) {
        EXPECT_EQ(err->reason(), ParseErrorReason::Io);
        EXPECT_EQ(err->path(), path);
        expectContains(err->what(), path.string());
    }
}

TEST(ParseErrorTest, ParseFilePropagatesIoError) {
    const std::optional<ParseError> err =
        thrownBy([&] { Parser{}.parseFile(fixture("does-not-exist.arxml")); });
    ASSERT_TRUE(err.has_value());
    if (err.has_value()) {
        EXPECT_EQ(err->reason(), ParseErrorReason::Io);
    }
}

TEST(ParseErrorTest, UnclosedTagReportsSyntaxWithLineAndColumn) {
    const auto path = fixture("malformed_unclosed.arxml");
    const std::optional<ParseError> err =
        thrownBy([&] { loadRawDocument(path); });
    ASSERT_TRUE(err.has_value());
    if (err.has_value()) {
        EXPECT_EQ(err->reason(), ParseErrorReason::Syntax);
        EXPECT_EQ(err->path(), path);
        const std::string message = err->what();
        expectContains(message, path.string());
        expectContains(message, "line 4");
        expectContains(message, "column 1");
        expectContains(message, "AR-PACKAGES");
    }
}

TEST(ParseErrorTest, ParseFilePropagatesSyntaxError) {
    const std::optional<ParseError> err = thrownBy(
        [&] { Parser{}.parseFile(fixture("malformed_unclosed.arxml")); });
    ASSERT_TRUE(err.has_value());
    if (err.has_value()) {
        EXPECT_EQ(err->reason(), ParseErrorReason::Syntax);
    }
}
