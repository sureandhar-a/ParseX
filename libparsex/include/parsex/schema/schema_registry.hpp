#pragma once

#include <string>

#include <parsex/schema/schema_resolution_result.hpp>

// Forward declaration — the full type lives in
// <parsex/telemetry/operation_telemetry.hpp> (interim seam until the
// Telemetry component lands). Only a pointer is taken, so this suffices.
struct OperationTelemetry;

// Resolves the shared XSD schema handle for an AUTOSAR release string
// (e.g. "4.2.2", "R21-11").
//
// Cache-first: if cachePathFor(release) already exists it is parsed directly
// (wasCacheHit=true) with no dependency on the schema sources. Otherwise the
// user-supplied source (<release>.xsd under $PARSEX_SCHEMA_DIR, the install
// default, or the source tree) is copied into the cache atomically and then
// parsed (wasCacheHit=false).
//
// Throws std::runtime_error when the release is unknown or a file cannot be
// parsed. (Interim: the dedicated SchemaResolutionError arrives in the
// error-handling subtask. Release-string validation against the known set
// arrives there too.)
//
// If telemetry is non-null, the parse step is timed as
// "SchemaRegistry.resolveSchema"; nullptr costs nothing.
SchemaResolutionResult resolveSchema(
    const std::string& release, OperationTelemetry* telemetry = nullptr);
