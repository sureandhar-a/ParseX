#pragma once

#include <string>
#include <parsex/raw/raw_span.hpp>
#include <optional>

struct CommonFields {
    std::string shortName;                  // required
    std::optional<std::string> category;    // optional
    RawSpan rawSpanRef;
};