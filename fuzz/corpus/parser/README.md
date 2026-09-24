# Parser fuzz seed corpus

Small valid inputs reused from the main test fixtures, kept tiny so
mutation stays fast. Large real-world inputs stay under `tests/fixtures`
and are not duplicated here.

Seeds:
- `loader_basic.arxml`, `loader_nested.arxml`, `tiny_valid.arxml`,
  `parsefile_complete.arxml`, `release_multiline.arxml`,
  `warnings_missing_fields.arxml` — valid parsing paths
- `malformed_unclosed.arxml`, `xxe_attack.arxml`, `xxe_ssrf.arxml` —
  expected-rejection paths (must not crash, must report cleanly)

Triage process:
- Run the direct-bytes entry with a scratch output dir first, e.g.
  `parsex_fuzz_parser /tmp/fuzzwork fuzz/corpus/parser -max_total_time=90`
- Any crash/hang/sanitizer report: minimize (`-minimize_crash`), fix at the
  source, and keep the minimized input here as a regression.
- Last generous run: ~483k execs in 91s, clean, no new inputs kept
  (mutants stayed in scratch dir by design).
