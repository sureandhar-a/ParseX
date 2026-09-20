#pragma once

#include <memory>
#include <libxml/tree.h>
#include "raw_node.hpp"

struct XmlDocDeleter {
    void operator()(xmlDoc* doc) const noexcept 
    { 
        if (doc) 
            xmlFreeDoc(doc); 
    }
};
using XmlDocPtr = std::unique_ptr<xmlDoc, XmlDocDeleter>;

class RawDocument {
public:
    explicit RawDocument(XmlDocPtr doc) : doc_(std::move(doc)) {}
    // Moves re-point the tree at its new home: RawNode's generated moves copy
    // child parent-pointers verbatim, so a moved tree must be relinked (this
    // previously orphaned every moved document's parent links — caught when
    // parseFile() started handing documents to shared_ptr).
    RawDocument(RawDocument&& other) noexcept
        : doc_(std::move(other.doc_)), root(std::move(other.root)) {
        relink(root);
    }
    RawDocument& operator=(RawDocument&& other) noexcept {
        if (this != &other) {
            doc_ = std::move(other.doc_);
            root = std::move(other.root);
            relink(root);
        }
        return *this;
    }
    RawDocument(const RawDocument&) = delete;
    RawDocument& operator=(const RawDocument&) = delete;
    xmlDoc* nativeHandle() const noexcept { return doc_.get(); }
    RawNode root;
private:
    static void relink(RawNode& node) {
        for (auto& child : node.children) {
            child->parent = &node;
            relink(*child);
        }
    }
    XmlDocPtr doc_;
};