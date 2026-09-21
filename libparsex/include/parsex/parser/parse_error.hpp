#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

// Failure reasons ParseX itself distinguishes (more to come as the
// error-handling work continues — e.g. XXE rejection reporting).
enum class ParseErrorReason { Io, Syntax };

// The dedicated Loader failure type: what went wrong (reason), where
// (path), and the specific detail (OS-level cause for I/O, libxml2's own
// message plus line/column for syntax). what() always renders all three so
// no failure is ever a bare "parse failed".
class ParseError : public std::runtime_error {
public:
    ParseError(ParseErrorReason reason, std::filesystem::path path, std::string detail)
        : std::runtime_error(buildMessage(reason, path, detail)),
          reason_(reason),
          path_(std::move(path)),
          detail_(std::move(detail)) {}

    ParseErrorReason reason() const noexcept { return reason_; }
    const std::filesystem::path& path() const noexcept { return path_; }
    const std::string& detail() const noexcept { return detail_; }

private:
    static std::string reasonName(ParseErrorReason reason) {
        return reason == ParseErrorReason::Io ? "Io" : "Syntax";
    }

    static std::string buildMessage(ParseErrorReason reason,
                                    const std::filesystem::path& path,
                                    const std::string& detail) {
        return "ParseError[" + reasonName(reason) + "] '" + path.string() +
               "': " + detail;
    }

    ParseErrorReason reason_;
    std::filesystem::path path_;
    std::string detail_;
};
