#pragma once

#include <string_view>

// ParseX build/tool version accessor (used as `toolVersion` in every JSON
// envelope; see schemas/envelope.schema.json).
//
// The value is defined once by the top-level CMake project VERSION and
// exposed here via the generated parsex/version_config.hpp, so JSON emitters
// never hardcode it. It moves together with the schema contract version in
// schemas/VERSIONING.md for the v1 milestone.
[[nodiscard]] std::string_view libparsexVersion();
