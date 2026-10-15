# Feature 1 — ARXML → DBC / CSV export (plan)

Target release: **v0.2.0** (additive: new CLI command, new MCP tool, new
envelope `kind`). Source idea: `ParseX-Next-Features.md` §Feature 1.

## What ships

- `parsex export --input <file.arxml> --format dbc|csv [--output <file>]`
- MCP tool `export_arxml` `{path, format}` → export text inline (read-only).
- Library API: `buildExportDatabase()`, `toDbc()`, `toCsv()` under
  `libparsex/include/parsex/export/`.

## Correction to the research doc

The doc says DBC is "just serialization". It is not. The domain model has
no CAN identifier (lives on `CAN-FRAME-TRIGGERING`), no scaling (lives
behind `COMPU-METHOD-REF`), no units (`UNIT`), and for real 4.x files the
bit packing lives on the PDU mapping, not the signal. DBC needs all of it.

Decision: resolve those joins in one export-owned step
(`buildExportDatabase`) that reads the model **plus** `ParsedFile::rawDocument`.
The domain model, JSON contract for `parse`, diff and write engines are
untouched — zero regression surface there. Promoting CAN ID / scaling into
the model is a later refactor if other features need them.

## Architecture

```
ParsedFile (model + rawDocument)
        │  buildExportDatabase()   ← S1, the only place joins happen
        ▼
ExportDatabase {frames[id, ext, fd, len, tx, signals[...]], nodes, warnings}
        │                    │
   toDbc()  ← S2        toCsv()  ← S3        (pure formatters)
        └────────┬───────────┘
   parsex export / export_arxml  ← S4 (CLI + MCP + JSON contract)
```

Interface is frozen in `libparsex/include/parsex/export/*.hpp` (slice S0).
Slices only touch the files they own, so they run in parallel without
conflicts.

## Mapping rules (shared reference)

| DBC / CSV field | ARXML 4.x source | Study vocabulary fallback |
|---|---|---|
| frame id, extended | `CAN-FRAME-TRIGGERING/IDENTIFIER`, `CAN-ADDRESSING-MODE=EXTENDED`, matched by `FRAME-REF` short name | none → frame skipped + warning |
| CAN FD | triggering `CAN-FRAME-TX-BEHAVIOR` or `-RX-BEHAVIOR` = `CAN-FD` | false |
| length | `Frame.length` | same |
| start bit | `I-SIGNAL-TO-I-PDU-MAPPING/START-POSITION` + `PDU-TO-FRAME-MAPPING/START-POSITION` (both bits; used verbatim: LSB for little endian, MSB for big endian — same numbering as DBC, matches cantools) | `PduSignalMapping.startPosition` |
| byte order | `PACKING-BYTE-ORDER` (`MOST-SIGNIFICANT-BYTE-FIRST` → big, else little) | `PduSignalMapping.byteOrder` |
| bit length, signed, init | `Signal` (I-SIGNAL) model fields | same |
| factor/offset | `I-SIGNAL/SYSTEM-SIGNAL-REF` → `SYSTEM-SIGNAL/.../COMPU-METHOD-REF` → linear `COMPU-SCALE/COMPU-RATIONAL-COEFFS`: offset = num[0]/den, factor = num[1]/den | `Signal.factor/offset` |
| min/max | linear scale `LOWER-LIMIT/UPPER-LIMIT` × factor + offset | `Signal.min/max` |
| value table | `TEXTTABLE` / `SCALE_LINEAR_AND_TEXTTABLE` scales with `COMPU-CONST/VT` where lower == upper | `Signal.valueTable` |
| unit | `SYSTEM-SIGNAL/.../UNIT-REF` or `COMPU-METHOD/UNIT-REF` → `UNIT/DISPLAY-NAME` (`NoUnit` → "") | `Signal.unit` |
| transmitters / receivers | `FRAME-PORT` (`COMMUNICATION-DIRECTION` OUT/IN) under an `ECU-INSTANCE`, referenced by the triggering's `FRAME-PORT-REFS`; and `ECU-INSTANCE/ASSOCIATED-COM-I-PDU-GROUP-REFS` → `I-SIGNAL-I-PDU-GROUP` direction + `I-SIGNAL-I-PDU-REF`s | `Frame.transmitters`, `Signal.receivers` |

All refs join by short name (last path segment) — same accepted v1
limitation as the model builder.

Out of scope (warn, don't fail): multiplexed / container / secured PDUs,
signal groups, CAN FD BRS attributes beyond `VFrameFormat`, `CM_` comments,
LIN/FlexRay/Ethernet clusters. Multiplexing needs `MULTIPLEXED-I-PDU`
parsing — separate feature.

## Slices

| Slice | Owner model | Owns (only these files) | Depends on |
|---|---|---|---|
| S0 interface + scaffold | lead | `parsex/export/*.hpp`, stub `.cpp`s, CMake wiring, this plan | — |
| S1 resolver | Sonnet | `libparsex/src/export/can_db.cpp`, `tests/libparsex/export/can_db_test.cpp` | S0 |
| S2 DBC writer | Sonnet | `libparsex/src/export/dbc_writer.cpp`, `tests/libparsex/export/dbc_writer_test.cpp` | S0 |
| S3 CSV writer | Haiku | `libparsex/src/export/csv_writer.cpp`, `tests/libparsex/export/csv_writer_test.cpp` | S0 |
| S4 CLI + MCP + contract | Sonnet | `parsex-cli/*`, `parsex-mcp/*`, `schemas/*`, `tests/cli/*`, `tests/mcp/*`, `tests/libparsex/json_contract/*` | S0 |
| S5 integration + cantools oracle | lead | `tests/libparsex/export/export_e2e_test.cpp`, `scripts/check_dbc_against_cantools.py` | S1–S4 merged |
| S6 docs | Haiku, lead review | `README.md`, `CHANGELOG.md`, `docs/cli.md`, `docs/mcp.md`, `docs/spec.md`, `docs/hld.md`, `docs/lld.md`, `schemas/README.md` | S5 |

S1–S4 run in parallel, each in its own git worktree and build dir.

### S2 DBC output contract

```
VERSION ""

NS_ :
	<standard NS_ keyword list>

BS_:

BU_: <nodes space-separated>

BO_ <id | 0x80000000 if extended> <name>: <length> <first transmitter or Vector__XXX>
 SG_ <name> : <startBit>|<bitLength>@<1 little|0 big><+ unsigned|- signed> (<factor>,<offset>) [<min>|<max>] "<unit>" <receivers comma-separated or Vector__XXX>

BA_DEF_ BO_ "VFrameFormat" ENUM ...;   (only when any frame is CAN FD)
BA_DEF_ "BusType" STRING ;             (only when any frame is CAN FD)
BA_DEF_DEF_ ...
BA_ "BusType" "CAN FD";
BA_ "VFrameFormat" BO_ <id> 14|15;     (FD frames: 14 standard, 15 extended)

VAL_ <id> <signal> <value> "<label>" ... ;
```

Numbers: shortest round-trip, `.` decimal, locale-free (`std::to_chars`).
Missing min/max → `[0|0]`. Frame/signal/node names that are not C
identifiers are sanitised (`[^A-Za-z0-9_]` → `_`, leading digit prefixed
with `_`); AUTOSAR short names already are, so this is a guard only.
`"` and `\` in unit/labels are escaped with `\`.

### S3 CSV output contract

RFC 4180, CRLF, header row:

```
frame,frame_id,extended,can_fd,length,transmitters,signal,start_bit,bit_length,byte_order,signed,factor,offset,min,max,unit,init_value,receivers,value_table
```

`frame_id` as `0x` hex upper-case; booleans `true/false`; `byte_order`
`little_endian|big_endian`; list cells `;`-joined; `value_table` as
`0=Off;1=On`; empty cell for absent optionals; a frame with no signals
still emits one row with the signal columns empty.

### S4 surface contract

- CLI: `parsex [--json] export --input/-i <file> --format/-f dbc|csv [--output/-o <file>]`.
  No `-o` → text to stdout. `-o` → written via `writeCacheAtomically`.
  Warnings → stderr as `parsex: warning: <msg>` (human mode).
  `--json` → envelope `kind: "exportResult"`, payload
  `{sourcePath, format, output|null, counts{frames,signals}, warnings[{message}], content?}`
  (`content` only when no `-o`).
- MCP `export_arxml`: input `{path, format: enum[dbc,csv]}`, read-only
  annotations, `structuredContent` = same envelope with `content`,
  `summary` = one-line human summary.
- `schemas/envelope.schema.json` gains `exportResult` + new
  `schemas/export_result.schema.json`.

## Verification gates

1. Each slice: own unit tests green, full `ctest` green in its worktree.
2. Lead review of each diff before merge.
3. S5: `system-4.2.arxml` (cantools' own fixture) → our DBC → loaded by
   cantools 44 and compared frame-by-frame / signal-by-signal against
   cantools loading the ARXML directly (id, extended, length, start, length,
   byte order, signedness, factor, offset, unit, choices).
4. Full `ctest`, asan preset, clang-tidy target on the branch; CI green on PR.

## Known pre-existing flakes (not caused by this work)

- `ResolveSchemaTest.FirstCallIsCacheMissSecondIsHit` and MCP approval
  tests can collide under `ctest -j`; rerun failures with `-j1`.
