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
    xmlDoc* nativeHandle() const noexcept { return doc_.get(); }
    RawNode root;
private:
    XmlDocPtr doc_;
};