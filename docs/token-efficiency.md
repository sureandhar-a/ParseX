# Token efficiency (living benchmark)

This is the evolving proof that ParseX consumes less model context than
pasting raw ARXML. Methodology is frozen per version (`Methodology v1`
below); results are append-only per release. Numbers are measured with
the real tokenizer against the built binary — never estimated with
`chars/4`.

- TL;DR (v0.1.0 baseline, `system-4.2.arxml`, 71,059 B):
  `parse` 22,059 → 209 tok (**105.5×**), `diff` 22,179 → 1,546 tok
  (**14.3×**), `validate` 22,059 → 10,002 tok (**2.2×** on a noisy file
  with 87 findings; 4.7× on a clean file). Schema cost for all 4 tools:
  **640 tok** total.
- Reproduce: `python3 scripts/measure_tokens.py` (see *Reproducing*).
- Prices are inputs, not constants: convert tokens to dollars at report
  time from the provider page and record the date (see *Results log*).

## Why this document looks like this

Token-proof docs that survive scrutiny converge on the same shape, so
this one copies it deliberately:

- **Control vs treatment** (Twilio `twilio-labs/mcp-te-benchmark`): the
  control is the traditional workflow, the treatment is the MCP path,
  same tasks both sides, metrics extracted by a script into a JSON
  artifact plus a summary table.
- **Frozen methodology, append-only results**
  (Scalekit `scalekit-inc/mcp-vs-cli-benchmark/METHODOLOGY.md`):
  hypotheses and counting rules are committed *before* running; each
  release appends a dated entry instead of editing old numbers. Their
  README reports per-task CLI-vs-MCP tokens/calls/time in one table —
  the same shape as the log below.
- **Before / After / Reduction tables** (GitHub `github-mcp-server`
  discussion #1182: 101 → 52 tools, 64.6k → 30.3k tok, 53%): one row
  per task, no prose hiding the denominator.
- **One normalized metric + per-run artifact** (GitHub agentic-savings
  write-up via InfoQ, May 2026): an Effective-Tokens style normalizer
  plus a `token-usage.jsonl`-equivalent artifact. Here the artifact is
  the `--out` JSON from `scripts/measure_tokens.py`.
- **Separate schema bloat from response bloat** (StackOne, Speakeasy,
  Apideck, Anthropic code-execution write-ups): tool *definitions* and
  tool *outputs* are different costs with different fixes. ParseX wins
  on both — 4 lean tools (schema) and counts-only reports (response).
- **Disclose everything or it is not a measurement** (IETF
  `draft-gaikwad-llm-benchmarking-methodology`, TechnoLynx 2026):
  tokenizer, binary version, commit, date, workload, failures recorded
  rather than dropped. Meta's cost-projection guide adds: never hardcode
  prices; record the price date and version them as inputs.

## Methodology v1 (frozen — do not edit, supersede with v2)

1. **Workload-anchored fixtures.** The same files the rest of the docs
   use: `tests/fixtures/schema_valid.arxml` (327 B),
   `parsefile_complete.arxml` (2,590 B), `tiny_valid.arxml` (3,780 B),
   `system-4.2.arxml` (71,059 B), plus the diff pair
   `schema_valid → system-4.2`. No synthetic files in the baseline.
2. **Control:** the whole `.arxml` file placed in model context
   (the naive `Read`/paste workflow). For `diff`, the control is the
   concatenation of both files.
3. **Treatment:** one `parsex-mcp` tool call over stdio
   (`tools/call` with per-request `_meta` protocol `2026-07-28`) and
   only its JSON-RPC response enters context. `tools/list` (640 tok)
   is reported separately as a once-per-session schema cost.
4. **Counting:** `tiktoken o200k_base` when installed (exact).
   Fallback `chars/3` only when tiktoken is absent, and the artifact
   records which method was used. Rationale: `1 tok ≈ 4 chars` is
   English-prose only; measured ARXML density here is 2.7–4.3
   chars/tok (Netra 2026: JSON `k=2.7`, code `k=2.8`; OpenAI: prose
   `≈4`), so `chars/4` undercounts XML cost by ~25–45%.
5. **Metrics per row:** raw bytes/tokens, tool bytes/tokens,
   `saving_pct = (1 − tool/raw) × 100`, `reduction_x = raw/tool`.
6. **Failures are rows, not omissions.** Tool errors (`isError:true`,
   e.g. `tiny_valid.arxml` parse under an unsupported release) are
   reported with their token cost, never silently dropped.
7. **Projections are labeled projections.** Anything beyond measured
   fixtures (e.g. 1 MB files) is derived from the measured density and
   the constant-size `parse` payload, and says so.
8. **Third-party files are fetched, never vendored.** Real-world entries
   record URL, commit, license, and SHA-256; the files stay out-of-tree
   so no rights or bloat enter the repo.

## Results log (append-only — add a dated entry per release, never rewrite)

### v0.1.0 baseline — 2026-09-25, rev `15f0f66`, binaries `0.1.0`

Artifact: regenerate with `python3 scripts/measure_tokens.py --out /tmp/tokens-0.1.0.json`.
Counting: `tiktoken o200k_base (exact)`, protocol `2026-07-28`.

| task | raw tok | tool tok | saved | reduction |
|---|---|---|---|---|
| tools/list (one-time schema cost) | — | 640 | — | — |
| parse_arxml schema_valid | 120 | 187 | −55.8% | 0.6x |
| validate_arxml schema_valid | 120 | 89 | 25.8% | 1.3x |
| parse_arxml parsefile_complete | 790 | 191 | 75.8% | 4.1x |
| validate_arxml parsefile_complete | 790 | 554 | 29.9% | 1.4x |
| parse_arxml tiny_valid | 877 | 69 | 92.1% | 12.7x |
| validate_arxml tiny_valid | 877 | 188 | 78.6% | 4.7x |
| parse_arxml system-4.2 | 22,059 | 209 | 99.1% | 105.5x |
| validate_arxml system-4.2 (87 findings) | 22,059 | 10,002 | 54.7% | 2.2x |
| diff_arxml schema_valid → system-4.2 | 22,179 | 1,546 | 93.0% | 14.3x |

Reading the table honestly:

- `parse` output is **O(1)** — release + 6 counts + warnings
  (`parsex-mcp/tools.hpp`) — so savings grow with file size. The only
  negative row is a 327 B toy file where the fixed envelope dominates;
  break-even is ≈ 600 B, irrelevant for real ARXML.
- `validate`/`diff` scale with *finding count*, not file size: 2.2× on
  the noisy 87-finding fixture, 4.7× on a clean file. The agent still
  gets actionable findings instead of a wall of XML to re-scan.
- Session view (schema amortized): one parse question 22,059 vs
  640 + 209 = 849 tok (**26×**); a 5-turn conversation revisiting the
  file 110,295 vs 1,685 tok-turns (**65.5×**).

Per-tool schema cost (`tools/list` breakdown, same run): `parse_arxml`
173 tok, `validate_arxml` 176, `diff_arxml` 172, `write_arxml` 256 —
total 640 tok, ~27× leaner than GitHub's MCP server (17,600 tok for 94
tools; industry typical 500–1,400 tok/tool).

CLI vs MCP (same engine, same envelope): `parsex-cli --json parse` on
`system-4.2.arxml` is 505 B vs the MCP response 642 B (+137 B JSON-RPC
envelope, +27%); human `parse` is 208 B. CLI is cheapest per byte —
use it for scripts/CI; MCP's value is agentic invocation for ~137 B
of overhead against a 71,059 B file.

### Real-world files — 2026-09-25, same binaries (`0.1.0`), same method

Beyond fixtures: full-fledged third-party ARXML downloaded from GitHub
and measured with the same harness (files kept out-of-tree — never
vendored; provenance below). The standout is a **736 KB CAN matrix
with 1,162 real signals** — the "trivial zeros" objection does not apply.

| file (source) | size | raw tok | parse tok | parse saving | validate tok | validate saving |
|---|---|---|---|---|---|---|
| `ARXMLContainerTest.arxml` ([canmatrix](https://github.com/ebroecker/canmatrix), 4.3.0, 1 cluster / 1 ECU / 5 frames / 13 PDUs / **1,162 signals**) | 736,028 B | 208,182 | 193 | 99.9% / **1,078.7×** | 95,966 | 53.9% / 2.2× |
| `Win_Ctrl.arxml` ([AutoToolMD](https://github.com/hnu-esnl/AutoToolMD), 4.2.2, SWC) | 973,807 B | 189,390 | 183 | 99.9% / **1,034.9×** | 114,339 | 39.6% / 1.7× |
| `IntLamp_ComCnvRx.arxml` (AutoToolMD, 4.2.2, COM-spec) | 479,007 B | 93,889 | 189 | 99.8% / 496.8× | 60,825 | 35.2% / 1.5× |
| `Win_ComCnvTx.arxml` (AutoToolMD, 4.2.2, COM-spec) | 298,276 B | 59,171 | 187 | 99.7% / 316.4× | 38,325 | 35.2% / 1.5× |
| `ARXML_min_max.arxml` (canmatrix, 4.2.2, 1 frame / 12 signals) | 35,320 B | 9,634 | 191 | 98.0% / 50.4× | 7,499 | 22.2% / 1.3× |

Provenance (fetch, don't vendor): canmatrix `development`
`2d0ebfd5` (2026-08-17), BSD-2-Clause; AutoToolMD `master`
`c478685f` (2025-12-17), no machine-readable license file in repo —
numbers and URLs only, no redistribution. SHA-256 of measured copies:
`1261fdc8…9e2629` (ARXMLContainerTest), `bc905641…b229fd67`
(Win_Ctrl), `4a1552d4…9a51adca` (IntLamp_ComCnvRx),
`a8be1715…f9223849` (Win_ComCnvTx).

Reading this table honestly:

- The 736 KB CAN file at 208,182 raw tokens **exceeds GPT-4o's 128k
  context pasted raw** — impossible without chunking (which breaks
  ARXML cross-references); via ParseX it is 193 tokens ($0.62 →
  $0.0006/query at Sonnet $3/MTok in, Sep 2026 pricing).
- SWC/COM-spec files parse to zero CAN counts — expected and
  documented scope (ParseX models 6 CAN types; see README Known
  limitations), not a tool failure. The agent still learns release +
  "no CAN elements" for ~190 tok instead of 40k–190k.
- `validate` on real files is 1.5–2.2×: findings lists dominate
  (unrecognized `DEST`s in study-vocabulary files). Modest, but the
  output is actionable findings, not raw XML.
- Excluded, not hidden: 7 other real downloads (releases 3.2.3, 4.0.3,
  4.1.1/4.1.2/4.1.3, 4.2.1, plus one lowercase `autosar_4-3-0.xsd`
  filename the exact-match release gate rejects) fall outside the
  supported 4.2.2–R21-11 range and return `isError` instead of a
  report. Release coverage, not token behavior, is the limit — tracked
  separately from this benchmark.

### Scale projection (labeled projection, not measurement)

Raw tokens scale linearly at the measured 3.22 chars/tok; `parse`
stays ≈ 209 tok. Cost at Claude Sonnet **$3 / 1M input tok**
(Anthropic pricing page, Sep 2026 — re-verify before reuse; GPT-4o
$2.50 / 1M is the same order).

| file size | raw tok (proj.) | parse tok | reduction | $/query raw → parsex |
|---|---|---|---|---|
| 0.07 MB (measured) | 22,785 | 209 | 109× | $0.068 → $0.00063 |
| 0.5 MB | ~162,755 | 209 | 779× | $0.488 → $0.00063 |
| 1 MB | ~325,511 | 209 | 1,557× | $0.977 → $0.00063 |
| 5 MB | ~1,627,558 | 209 | 7,787× | $4.88 → $0.00063 |

A 1 MB file already overflows a 128k-token context pasted raw; via
ParseX it is 209 tokens. Real ARXML runs to thousands–tens of
thousands of lines per file and hundreds of files per project, so the
right side of this table is the realistic operating point.

## Reproducing

```bash
cmake --preset default && cmake --build --preset default
python3 scripts/measure_tokens.py --out /tmp/tokens.json
# without tiktoken the script uses the documented chars/3 fallback;
# for exact counts: python3 -m pip install tiktoken
```

Compare any two entries by re-running the script on the same commit
and diffing the `--out` artifacts. Keep the artifact alongside the
release notes when refreshing this log.

## Limitations (honest scope)

- Tokenizer varies ±10–30% across model families; this log uses
  `o200k_base`. Re-count with the provider's counter for billing-grade
  numbers.
- End-to-end session tokens (API `usage.input_tokens`) are not logged
  here — these are exact payload counts, a proxy for the billed total.
- `tiny_valid.arxml` parse fails under an unsupported release and is
  reported as an `isError` row; parse/validate savings assume a
  parseable release.
- The MCP server speaks stateless `2026-07-28` with per-request
  `_meta` and no `initialize` handshake (see `docs/mcp.md`), so some
  hosts need compat handling before the schema cost applies.

## This document's changelog

- 2026-09-25: created with Methodology v1 + v0.1.0 baseline (rev
  `15f0f66`); extended same day with real-world third-party files
  (canmatrix BSD-2-Clause, AutoToolMD) incl. negative-scope notes.
  Next entry appends a dated table per release; a counting
  or workload change supersedes with Methodology v2, never by editing
  v1 rows.

## Cross-links

- `docs/mcp.md` — tools, protocol version, launch config.
- `docs/cli.md` — the CLI twin sharing engines and report shapes.
- `scripts/measure_tokens.py` — the harness generating the tables above.
- `schemas/VERSIONING.md`, `docs/versioning.md` — what counts as a
  breaking change when a new entry is added.
