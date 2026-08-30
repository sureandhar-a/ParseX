#include <string>
#include <vector>
#include "raw_span.hpp"


struct RawNode {
    std::string tagName;
    std::vector<std::pair<std::string, std::string>> attributes;
    RawSpan span;
    RawNode* parent = nullptr;                        // non-owning
    std::vector<std::unique_ptr<RawNode>> children;    // owning
};