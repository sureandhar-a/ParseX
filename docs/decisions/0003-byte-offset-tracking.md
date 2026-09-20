# 0003 — Byte-offset tracking via SAX in-callback anchors (reader API rejected)

(Numbered 0003 because 0002 is already taken by the vendored-schemas decision.)

- Context: the Parser's Span-Tracking Loader must record a per-element byte span
  (`RawSpan` start/end offsets) for the Write Engine's later use. Open question from
  the Parser feature: does libxml2's reader API — `xmlTextReaderByteConsumed()`
  polled in an `xmlReaderForFile()` + `xmlTextReaderRead()` loop — give reliable byte
  offsets, or is a custom input-callback byte counter needed instead? Measured on
  libxml2 2.9.13 with the throwaway probes in `spikes/` (`byte_offset_spike.cpp` for
  the reader loop, `sax_pos_spike.cpp` for SAX callbacks) against three fixtures with
  known-true offsets (`content.find(b'<TAG')`): `fixture_utf8.arxml` (UTF-8, 170 bytes:
  `<AUTOSAR>`@39 `<AR-PACKAGES>`@51 `<AR-PACKAGE>`@69 `<SHORT-NAME>`@88),
  `fixture_latin1.arxml` (ISO-8859-1, 206 bytes, `<!-- Müller café -->` with two
  non-ASCII bytes placed *before* the measured elements: `<AUTOSAR>`@44
  `<AR-PACKAGES>`@87 `<AR-PACKAGE>`@105 `<SHORT-NAME>`@124), and `fixture_utf16.arxml`
  (UTF-16LE + BOM, 344 bytes: `<AUTOSAR>`@82 `<AR-PACKAGES>`@106 `<AR-PACKAGE>`@142
  `<SHORT-NAME>`@180).

- Decision: do NOT use `xmlTextReaderByteConsumed()` from a reader loop. Capture spans
  from SAX2 start/end-element callbacks, using in-callback `xmlByteConsumed()` as the
  end anchor plus a backward scan for `<` (the Veillard technique), with
  encoding-aware length conversion. No custom input-callback byte counter.

- Reasoning: the reader API fails even for plain UTF-8, so neither "use as-is" nor
  "use with UTF-8-only constraint" is viable. Small fixtures: *every* element start
  reports the whole file size — UTF-8: 170, 170, 170, 170 vs true `<` at 39/51/69/88;
  latin-1: 206 × 4 vs true 44/87/105/124; UTF-16: 344 × 4 vs true 82/106/142/180 —
  and the reader line number is stuck at the last line (9/10/9). Large fixtures show
  why: the value steps with the input buffer and is shared by dozens of nodes (big
  latin-1, 162954 bytes / 2000 elements: 515, 1006, 1538, … 162954; 46 nodes share
  `consumed=515`; big UTF-8: first 14 nodes all report 513). It is a buffer-fill
  pointer, not a node position. Transcoding is *not* the failure: the big latin-1
  file holds 6000 `é` (1 byte raw, 2 bytes transcoded, so a transcoded count would end
  at 168954), yet final `consumed` is 162954 — the exact raw file size. On 2.9.13 the
  counter tracks raw input bytes; buffering alone disqualifies the reader API.
  In-callback `xmlByteConsumed()`, by contrast, is exact in all three encodings with a
  fixed ±1 rule. UTF-8 START: 47, 63, 80, 99 == true end-`>` offsets exactly (points
  AT `>`); END: 123, 141, 158, 169 == true + 1 (just past `>`). Latin-1 START: 52, 99,
  116, 135 — exact, zero drift despite the preceding non-ASCII comment; END again
  exactly +1. UTF-16LE START: 98, 130, 164, 202 — each the first byte of the 2-byte
  `>` (naive deltas vs `<` of +16/+24/+22/+22 look like drift but the end-anchor rule
  is fixed); END exactly +1 past the final `>` byte. Callback line/column and DOM
  `xmlGetLineNo()` are likewise exact in every encoding. This matches libxml2 author
  Daniel Veillard's answer to exactly this question
  (https://mail.gnome.org/archives/xml/2012-October/msg00018.html — anchor just after
  the start tag's `>` in the callback, walk backward from `ctxt->input->cur` in
  `ctxt->input->base` for `<`, convert the UTF-8 length back to the document encoding
  before subtracting) to within a one-byte AT-vs-PAST-`>` version detail.

- Consequences: the Loader must capture spans from SAX callbacks, not from a reader
  loop (a reader-driven walk can still do structure, but span anchors come from
  callbacks). No UTF-8-only user-facing caveat is needed — spans are exact in every
  tested encoding, so there is no deferred non-UTF-8 work item from this decision.
  No custom I/O byte-counter code to write or maintain: in-callback `consumed`
  already counts raw file bytes. The backward `<` scan must be encoding-aware
  (UTF-16: 2-byte units; multi-byte single-byte-encodings need the Veillard
  length conversion). Revisit triggers: a libxml2 version change (re-run the
  `spikes/` probes — full measurements in `spikes/byte_offset_NOTES.md`) or a user
  splicing bug report (this ADR preserves the original numbers so the spike need not
  be redone).
