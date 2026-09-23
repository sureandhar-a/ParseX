#include <parsex/write/write_context.hpp>

#include <cstdlib>
#include <libxml/xmlstring.h>

namespace {

std::string defaultSchemaFilename() {
    // Matches tests/fixtures/parsefile_complete.arxml and system-4.2.arxml:
    // r4.0 namespace with the 4.4.0 distribution file.
    return "AUTOSAR_00046.xsd";
}

}  // namespace

WriteContext::WriteContext(const std::string& namespaceUri,
                           const std::string& schemaLocation)
    : doc_(XmlDocPtr(xmlNewDoc(BAD_CAST "1.0"))),
      namespaceUri_(namespaceUri),
      schemaLocation_(schemaLocation) {
    if (doc_ == nullptr) {
        throw std::runtime_error("parsex: cannot create XML document for WriteContext");
    }
    root_ = xmlNewNode(nullptr, BAD_CAST "AUTOSAR");
    if (root_ == nullptr) {
        throw std::runtime_error("parsex: cannot create AUTOSAR root element");
    }
    xmlDocSetRootElement(doc_.get(), root_);
    // Default namespace (nullptr prefix) per TPS_ASR_00017.
    xmlNsPtr defaultNs = xmlNewNs(root_, BAD_CAST namespaceUri_.c_str(), nullptr);
    if (defaultNs == nullptr) {
        throw std::runtime_error("parsex: cannot declare AUTOSAR default namespace");
    }
    xmlSetNs(root_, defaultNs);
    // xsi namespace with explicit prefix for the schema-location attribute.
    xmlNsPtr xsiNs =
        xmlNewNs(root_, BAD_CAST "http://www.w3.org/2001/XMLSchema-instance", BAD_CAST "xsi");
    if (xsiNs == nullptr) {
        throw std::runtime_error("parsex: cannot declare xsi namespace");
    }
    if (xmlNewNsProp(root_, xsiNs, BAD_CAST "schemaLocation",
                     BAD_CAST schemaLocation_.c_str()) == nullptr) {
        throw std::runtime_error("parsex: cannot set xsi:schemaLocation");
    }
    // Single AR-PACKAGES child: attachment point for domain content.
    arPackages_ = xmlNewChild(root_, nullptr, BAD_CAST "AR-PACKAGES", nullptr);
    if (arPackages_ == nullptr) {
        throw std::runtime_error("parsex: cannot create AR-PACKAGES element");
    }
}

WriteContext WriteContext::forRelease(const std::string& autosarRelease) {
    const std::string ns = namespaceUriForRelease(autosarRelease);
    return WriteContext(ns, ns + " " + schemaFilenameForRelease(autosarRelease));
}

std::string WriteContext::dumpToString() const {
    xmlChar* buffer = nullptr;
    int size = 0;
    xmlDocDumpFormatMemory(doc_.get(), &buffer, &size, 1);
    if (buffer == nullptr) {
        return "";
    }
    std::string out(reinterpret_cast<const char*>(buffer), static_cast<std::size_t>(size));
    xmlFree(buffer);
    return out;
}

std::string WriteContext::namespaceUriForRelease(const std::string& autosarRelease) {
    // AUTOSAR 4.x schemas share the r4.0 namespace (both project fixtures use
    // r4.0 with AUTOSAR_00046.xsd for release 4.4.0). R-releases map
    // mechanically: R21-11 -> r21.11.
    if (!autosarRelease.empty() && (autosarRelease.at(0) == 'R' || autosarRelease.at(0) == 'r')) {
        std::string rest = autosarRelease.substr(1);
        for (char& letter : rest) {
            if (letter == '-') {
                letter = '.';
            }
        }
        return "http://autosar.org/schema/r" + rest;
    }
    if (autosarRelease.rfind("4.", 0) == 0) {
        return "http://autosar.org/schema/r4.0";
    }
    // Fallback: dotted release -> r<major>.<minor> (e.g. "4.0" -> r4.0).
    const std::size_t firstDot = autosarRelease.find('.');
    if (firstDot != std::string::npos) {
        const std::size_t secondDot = autosarRelease.find('.', firstDot + 1);
        const std::string shortVersion = (secondDot == std::string::npos)
                                             ? autosarRelease
                                             : autosarRelease.substr(0, secondDot);
        return "http://autosar.org/schema/r" + shortVersion;
    }
    return "http://autosar.org/schema/r" + autosarRelease;
}

std::string WriteContext::schemaFilenameForRelease(const std::string& autosarRelease) {
    // Inverse of release_detector's schemaFilenameTable (AUTOSAR distribution
    // names). Unknown versions fall back to the default fixture file rather
    // than throwing here — release validation owns unknown-release errors.
    if (autosarRelease == "4.2.2") {
        return "AUTOSAR_4-2-2.xsd";
    }
    if (autosarRelease == "4.3.0") {
        return "AUTOSAR_4-3-0.xsd";
    }
    if (autosarRelease == "4.3.1") {
        return "AUTOSAR_00044.xsd";
    }
    if (autosarRelease == "4.4.0") {
        return "AUTOSAR_00046.xsd";
    }
    if (autosarRelease == "R19-11") {
        return "AUTOSAR_00048.xsd";
    }
    if (autosarRelease == "R20-11") {
        return "AUTOSAR_00049.xsd";
    }
    if (autosarRelease == "R21-11") {
        return "AUTOSAR_00050.xsd";
    }
    return defaultSchemaFilename();
}

std::string WriteContext::schemaLocationForRelease(const std::string& autosarRelease) {
    const std::string ns = namespaceUriForRelease(autosarRelease);
    return ns + " " + schemaFilenameForRelease(autosarRelease);
}
