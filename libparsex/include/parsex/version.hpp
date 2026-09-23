#pragma once

#include <string_view>

// ParseX build/tool version accessor (used as `toolVersion` in every JSON
// envelope; see schemas/envelope.schema.json and PAR-174).
//
// Single place the tool version is defined (libparsex/src/version.cpp), so
// JSON emitters never hardcode it. Currently "1.0.0"; bumped by the build /
// release process, independently of `parsex::json_contract::kContractVersion`
// (the schema contract version in schemas/VERSIONING.md).
[[nodiscard]] std::string_view libparsexVersion();
