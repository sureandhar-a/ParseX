#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

// Why resolveSchema() failed.
enum class SchemaResolutionReason {
    UnsupportedRelease,  // no <release>.xsd in any schema source
    SchemaFileMissing,   // a needed file vanished or is unreadable
    SchemaFileCorrupt,   // libxml2 could not parse an existing file
    CacheWriteFailed     // populating the cache failed (served from source instead)
};

inline std::string_view reasonName(SchemaResolutionReason reason) {
    switch (reason) {
        case SchemaResolutionReason::UnsupportedRelease:
            return "UnsupportedRelease";
        case SchemaResolutionReason::SchemaFileMissing:
            return "SchemaFileMissing";
        case SchemaResolutionReason::SchemaFileCorrupt:
            return "SchemaFileCorrupt";
        case SchemaResolutionReason::CacheWriteFailed:
            return "CacheWriteFailed";
    }
    return "Unknown";
}

// The dedicated resolveSchema() failure type. Carries the requested release
// string and a machine-readable reason; what() always contains the release
// string plus libxml2's own error text where one exists.
class SchemaResolutionError : public std::runtime_error {
public:
    SchemaResolutionError(
        std::string release, SchemaResolutionReason reason, const std::string& detail)
        : std::runtime_error(
              std::string("SchemaResolutionError[") + std::string(reasonName(reason)) +
              "] release '" + release + "': " + detail),
          release_(std::move(release)),
          reason_(reason) {}

    const std::string& release() const noexcept { return release_; }
    SchemaResolutionReason reason() const noexcept { return reason_; }

private:
    std::string release_;
    SchemaResolutionReason reason_;
};
