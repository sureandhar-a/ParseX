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
// Throws SchemaResolutionError (see schema_resolution_error.hpp) carrying
// the requested release and a reason: UnsupportedRelease (no <release>.xsd
// in any source — the common typo/unsupported-string case),
// SchemaFileMissing (a needed file vanished or is unreadable), or
// SchemaFileCorrupt (libxml2 rejected an existing file, with its error text
// folded into the message). A cache-write failure is NOT fatal: the schema
// is served by parsing the source directly instead.
//
// If telemetry is non-null, the parse step is timed as
// "SchemaRegistry.resolveSchema"; nullptr costs nothing.
SchemaResolutionResult resolveSchema(
    const std::string& release, OperationTelemetry* telemetry = nullptr);
