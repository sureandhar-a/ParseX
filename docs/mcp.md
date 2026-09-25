# ParseX Bridge (`parsex-mcp`)

`parsex-mcp` is the assistant-facing surface around `libparsex` — alongside the command-line surface (`parsex`, see `docs/cli.md`). It exposes the Parser, Validator, Diff Engine, and Write Engine as callable tools over stdio, so an assistant can parse, validate, diff, and safely edit ARXML files directly in a conversation, without a human first running the command line by hand.

The two surfaces share engines and output shapes: each tool's `structuredContent` is the same report envelope the command line emits (`parseReport`, `validationResult`, `diffReport`, `writeResult` per the JSON Output Contract). If you know one, you know the other's results — see `docs/cli.md` for the human and `--json` forms.

This document is kept in sync with the actual `tools/list` output — no documented tool without an implementation, no implementation without documentation.

## Launching as a stdio server

The server speaks newline-delimited JSON-RPC over standard streams. Stdout is reserved for protocol messages only; diagnostics go to stderr.

Example assistant config entry (Claude Code / Claude Desktop style):

```json
{
  "mcpServers": {
    "parsex": {
      "command": "/path/to/parsex-mcp",
      "args": [],
      "env": {}
    }
  }
}
```

Manual smoke check:

```sh
printf '{"jsonrpc":"2.0","id":1,"method":"server/discover"}\n' | ./build/debug/parsex-mcp/parsex-mcp
# {"jsonrpc":"2.0","id":1,"result":{"resultType":"complete","supportedVersions":["2026-07-28"],...}}
```

## Protocol version

Supported version: `2026-07-28`, sent statelessly on every request:

```json
{"jsonrpc":"2.0","id":1,"method":"tools/list","_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28"}}
```

Identity reply (`server/discover`):

```json
{
  "resultType": "complete",
  "supportedVersions": ["2026-07-28"],
  "capabilities": {"tools": {"listChanged": false}},
  "_meta": {"io.modelcontextprotocol/serverInfo": {"name": "parsex-mcp", "version": "0.1.0"}}
}
```

## Tools

### `parse_arxml` — Parse an ARXML file and report its structure

Annotations: `readOnly:true, destructive:false, idempotent:true, openWorld:false`.

Input:

```json
{"type":"object","properties":{"path":{"type":"string"}},"required":["path"],"additionalProperties":false}
```

Output (`parseReport` envelope with `sourcePath, autosarRelease, counts, warnings`).

Example:

```sh
# request
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"parse_arxml","arguments":{"path":"tests/fixtures/schema_valid.arxml"}}}
# response (trimmed)
{"jsonrpc":"2.0","id":3,"result":{"resultType":"complete","content":[{"type":"text","text":"Parsed ..."}],"structuredContent":{"kind":"parseReport","payload":{...}}}}
```

### `validate_arxml` — Validate an ARXML file

Annotations: same as above (safe to retry).

Input: `path` required, `strict` optional boolean default `false` (strict makes warnings fail the verdict, mirroring `validate --strict`).

Output (`validationResult` envelope with `passed, errors[]`). A run that finds findings still succeeds — `passed:false` lives in the body, `isError` stays absent. Only genuine tool failures use `isError:true`.

Example strict vs lenient against the same file — sequencing matches the command line (parser first, then validator).

### `diff_arxml` — Diff two ARXML files

Annotations: same as above.

Input: `basePath` and `targetPath` required strings.

Output (`diffReport` envelope with `entries[], diagnostics[]`). Human text is the deterministic grouped rendering, or `No differences` when empty.

### `write_arxml` — Preview or apply a safe edit

> **Dry-run by default.** This tool previews without touching disk unless you explicitly pass `apply:true`. Show the user the preview and get explicit confirmation before retrying with `apply:true`.

Annotations: `readOnly:false, destructive:true, idempotent:false, openWorld:false` — the one higher-risk tool, gated by hosts that respect annotations.

Input: `path` and `outputPath` required, `apply` optional boolean default `false`.

Output (`writeResult` envelope with `input, output, applied, success`).

Dry-run (default):

```json
{"name":"write_arxml","arguments":{"path":"in.arxml","outputPath":"/tmp/out.arxml"}}
# -> {"applied":false,"success":true}, content starts "DRY RUN — no changes written..."
# no file is created or modified
```

Apply:

```json
{"name":"write_arxml","arguments":{"path":"in.arxml","outputPath":"/tmp/out.arxml","apply":true}}
# -> {"applied":true,"success":true}, content "Wrote /tmp/out.arxml from in.arxml"
```

Description surfaced to models states the preview default and confirmation step directly, so an assistant with no other context previews first.

## Limitations (honest scope)

* Version handling is scoped down: speaks stateless `2026-07-28` with per-request `_meta`, detects but does not support older `initialize`-handshake clients — they get a clear version error rather than silent misbehavior.
* Tools primitive only. `resources` and `prompts` are out of scope — engines map naturally onto callable actions, not browsable data.
* No `listChanged` notifications — the tool set is fixed at startup.
* Cancellation (`notifications/cancelled`) is best-effort: single-threaded, no concurrent calls, so a notice for a completed or unknown request is safely ignored. True mid-call interruption would need cooperatively cancellable engines.

## Testing

* Pipe tests (`ctest -R "transport.pipe|protocol.pipe"`) drive the built binary over real stdin/stdout.
* Golden transcripts (`ctest -R McpApproval`) record full sessions (`server/discover -> tools/list -> tools/call` plus per-tool, error, and version sessions). To regenerate after an intentional change: run, review `.received.txt`, copy over `.approved.txt` when correct.
* Sanitizers: `cmake --preset build-asan && ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/asan -R "transport.pipe|protocol.pipe|McpApproval|ReadTools|WriteTool|Dispatch"` — zero findings, including untrusted byte input.

## Cross-links

* `docs/cli.md` — the command-line twin sharing engines and report shapes; readers of one should read the other.
* Design references `docs/spec.md`, `docs/hld.md` (requirements, high-level design) mention the bridge abstractly — this file is the concrete reference for the implemented interface.
* Schemas live in `schemas/envelope.schema.json` and per-kind files; the bridge reuses `wrapEnvelope()` and never hardcodes versions.
