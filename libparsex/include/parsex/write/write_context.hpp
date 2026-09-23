#pragma once

#include <memory>
#include <string>

#include <libxml/tree.h>

#include <parsex/raw/raw_document.hpp>

// Write Engine root scaffold (PAR-144): owns the output xmlDoc plus the
// <AUTOSAR> root element (default namespace per TPS_ASR_00017, versioned URI
// per TPS_ASR_00011) with xsi:schemaLocation (bare filename per TPS_ASR_00013)
// and the single <AR-PACKAGES> child confirmed by real ARXML samples.
//
// Namespaces: AUTOSAR 4.x schemas share the r4.0 namespace
// (see tests/fixtures/parsefile_complete.arxml: r4.0 namespace with
// AUTOSAR_00046.xsd which resolves to release 4.4.0). R-releases use
// r<major>.<minor> derived from the release string (R21-11 -> r21.11).
struct WriteContext {
    // Direct construction with explicit URIs (tests use this).
    WriteContext(std::string namespaceUri, std::string schemaLocation);

    WriteContext(const WriteContext&) = delete;
    WriteContext& operator=(const WriteContext&) = delete;
    WriteContext(WriteContext&&) noexcept = default;
    WriteContext& operator=(WriteContext&&) noexcept = default;
    ~WriteContext() = default;

    // Convenience: build scaffold for a Schema Registry release string
    // (e.g. "4.4.0", "R21-11") using the helpers below.
    static WriteContext forRelease(const std::string& autosarRelease);

    [[nodiscard]] xmlDocPtr doc() const noexcept { return doc_.get(); }
    [[nodiscard]] xmlNodePtr root() const noexcept { return root_; }
    [[nodiscard]] xmlNodePtr arPackages() const noexcept { return arPackages_; }
    [[nodiscard]] const std::string& namespaceUri() const noexcept { return namespaceUri_; }
    [[nodiscard]] const std::string& schemaLocation() const noexcept {
        return schemaLocation_;
    }

    // Quick manual inspection during development (not the final file writer —
    // PBI 3 owns that). Dumps the current tree via xmlDocDumpFormatMemory.
    [[nodiscard]] std::string dumpToString() const;

    // Maps a Schema Registry release (see release_detector's table) to the
    // namespace URI written on the root element.
    static std::string namespaceUriForRelease(const std::string& autosarRelease);
    // Maps a release to its AUTOSAR distribution schema filename
    // (bare filename, no path, per TPS_ASR_00013).
    static std::string schemaFilenameForRelease(const std::string& autosarRelease);
    // Full xsi:schemaLocation value: "<namespaceUri> <schemaFilename>".
    static std::string schemaLocationForRelease(const std::string& autosarRelease);

private:
    XmlDocPtr doc_;
    xmlNodePtr root_ = nullptr;
    xmlNodePtr arPackages_ = nullptr;
    std::string namespaceUri_;
    std::string schemaLocation_;
};
