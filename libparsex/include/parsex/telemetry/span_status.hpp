#pragma once

// Foundational span status type for the Telemetry Feature (PAR-184).
//
// Mirrors the OpenTelemetry Trace API Status codes (Unset/Ok/Error); ParseX
// does not adopt SpanKind or Links since those describe distributed tracing.

namespace parsex::telemetry {

enum class SpanStatus { Unset, Ok, Error };

[[nodiscard]] const char* toString(SpanStatus status) noexcept;

}  // namespace parsex::telemetry
