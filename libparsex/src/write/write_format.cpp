#include <parsex/write/write_format.hpp>

#include <libxml/xmlsave.h>

#include <stdexcept>

namespace {

bool isWhitespaceOnly(const xmlChar* content) {
    if (content == nullptr) {
        return false;
    }
    for (const xmlChar* ptr = content; *ptr != '\0'; ++ptr) {
        if (*ptr != ' ' && *ptr != '\t' && *ptr != '\r' && *ptr != '\n') {
            return false;
        }
    }
    return true;
}

void checkNode(const xmlNode* node) {
    for (const xmlNode* child = node; child != nullptr; child = child->next) {
        if (child->type == XML_TEXT_NODE && isWhitespaceOnly(child->content)) {
            throw std::runtime_error(
                "parsex: whitespace-only text node found (would defeat the pretty-printer)");
        }
        if (child->children != nullptr) {
            checkNode(child->children);
        }
    }
}

}  // namespace

void assertNoWhitespaceOnlyTextNodes(const xmlNode* root) {
    if (root == nullptr) {
        return;
    }
    checkNode(root);
}

int writeXmlToFile(xmlDocPtr doc, const std::string& path) {
    if (doc == nullptr) {
        throw std::runtime_error("parsex: cannot write null XML document to '" + path + "'");
    }
    // format=1 enables indentation (XML_SAVE_FORMAT-equivalent for this
    // older-style API — chosen over xmlSaveToFilename + option flags because
    // the single call covers encoding + formatting with no extra state).
    const int written = xmlSaveFormatFileEnc(path.c_str(), doc, "UTF-8", 1);
    if (written < 0) {
        throw std::runtime_error("parsex: failed to write XML file '" + path +
                                 "' (unwritable path or I/O error)");
    }
    return written;
}
