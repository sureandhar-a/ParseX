#include <memory>
#include <libxml/tree.h>

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
private:
    XmlDocPtr doc_;
};