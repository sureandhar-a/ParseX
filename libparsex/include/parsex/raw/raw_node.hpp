#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "raw_span.hpp"


struct RawNode {
    std::string tagName;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::string text;  // Raw character content accumulated from SAX character
                       // data (entity-resolved, CDATA included, untrimmed:
                       // whitespace between child elements accumulates into
                       // the parent — readers trim). Empty when the element
                       // holds no character data (e.g. self-closed tags).
    RawSpan span;
    RawNode* parent = nullptr;                        // non-owning
    std::vector<std::unique_ptr<RawNode>> children;    // owning
};