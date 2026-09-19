#pragma once

#include <parsex/schema/cached_schema.hpp>

// The outcome of a SchemaRegistry::resolveSchema() call.
struct SchemaResolutionResult {
    CachedSchema schema;
    bool wasCacheHit = false;   // useful for telemetry/debugging, not required for correctness
};
