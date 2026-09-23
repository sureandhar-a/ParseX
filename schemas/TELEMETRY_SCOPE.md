# ParseX Telemetry Scope: Local Instrumentation Only (PAR-196)

ParseX's telemetry is internal performance instrumentation: span/trace timing
of the Parser, Validator, Diff Engine, and Write Engine phases so a maintainer
or user can diagnose slow runs on large ARXML files. It is not usage analytics,
is never transmitted anywhere, and stays entirely local to the machine running
ParseX.

## Why not phone-home analytics

ParseX has no backend server and no plan to build one. There is no privacy
policy or data-handling commitment in place. Building phone-home analytics
without either of those would be irresponsible, regardless of whether it
defaulted to opt-in or opt-out.

## Researched precedent (intentionally not copied as-is)

Real developer-tool telemetry is opt-out by default, each with a documented
disable mechanism and an explicit list of what is and is not collected:

* .NET CLI telemetry (opt-out via `DOTNET_CLI_TELEMETRY_OPTOUT`):
  https://learn.microsoft.com/en-us/dotnet/core/tools/telemetry
* Next.js telemetry (opt-out via `next telemetry disable`):
  https://nextjs.org/telemetry
* Homebrew analytics (opt-out via `HOMEBREW_NO_ANALYTICS`, upfront notice):
  https://docs.brew.sh/Analytics

ParseX intentionally does not copy this pattern today: all three precedents
assume the maintainer has real backend infrastructure and a published privacy
policy to receive and handle the data. ParseX has neither. If ParseX ever does
build phone-home analytics, this precedent is the right starting point — but
doing so requires first standing up the backend, publishing the privacy policy,
and re-opening this decision explicitly, not silently wiring the existing local
instrumentation to a network call.

## Getting the local telemetry data today

Run with telemetry enabled (`parsex::telemetry::TelemetryConfig::setEnabled(true)`,
or whatever CLI flag the CLI Feature exposes) and read the resulting
`telemetryReport` JSON from stdout or a file — see
[`telemetry_report.schema.json`](telemetry_report.schema.json) for the payload
shape. No server, no account, no network call is involved anywhere in the path.
