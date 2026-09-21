#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Thrown when LazyOnReference discovery cannot satisfy every cross-file
// short-name reference: either no discovered file defines the wanted name
// (unresolvable, possibly a typo or a file outside the searched trees) or
// the iteration cap was hit (pathological chains). Carries the still-missing
// names; what() lists them plus the searched directories, so the failure is
// actionable instead of a hang or a silent partial project.
class DanglingFileReferenceError : public std::runtime_error {
public:
    DanglingFileReferenceError(std::vector<std::string> refs,
                               std::vector<std::filesystem::path> searchedDirs,
                               std::string context = {})
        : std::runtime_error(buildMessage(refs, searchedDirs, context)),
          refs_(std::move(refs)),
          searchedDirs_(std::move(searchedDirs)) {}

    const std::vector<std::string>& refs() const noexcept { return refs_; }
    const std::vector<std::filesystem::path>& searchedDirs() const noexcept {
        return searchedDirs_;
    }

private:
    static std::string buildMessage(const std::vector<std::string>& refs,
                                    const std::vector<std::filesystem::path>& dirs,
                                    const std::string& context) {
        std::string message = "parsex: dangling file reference(s): ";
        for (std::size_t idx = 0; idx < refs.size(); ++idx) {
            if (idx > 0) {
                message += ", ";
            }
            message += "'" + refs.at(idx) + "'";
        }
        message += " — no parsed or discovered ARXML file defines them (searched:";
        for (const auto& dir : dirs) {
            message += " ";
            message += dir.string();
        }
        message += ")";
        if (!context.empty()) {
            message += " [unresolved sites: " + context + "]";
        }
        return message;
    }

    std::vector<std::string> refs_;
    std::vector<std::filesystem::path> searchedDirs_;
};
