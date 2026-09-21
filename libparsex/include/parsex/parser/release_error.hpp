#pragma once

#include <stdexcept>
#include <string>
#include <utility>

// Thrown when an ARXML file's schema filename maps to no supported AUTOSAR
// release. Carries the offending filename; what() always contains it, so a
// user can see exactly what their file declared.
class UnsupportedReleaseError : public std::runtime_error {
public:
    explicit UnsupportedReleaseError(std::string schemaFilename)
        : std::runtime_error("UnsupportedReleaseError: schema '" + schemaFilename +
                             "' is not a supported AUTOSAR release (supported: 4.2.2-R21-11)"),
          schemaFilename_(std::move(schemaFilename)) {}

    const std::string& schemaFilename() const noexcept { return schemaFilename_; }

private:
    std::string schemaFilename_;
};
