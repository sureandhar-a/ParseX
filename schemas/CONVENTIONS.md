# ParseX JSON Conventions

Checkable reference for every kind-specific payload schema. Review new schemas
(PAR-164 onward) against these three rules before merging.

## 1. Field naming: camelCase

**Rule:** All JSON object keys emitted by ParseX MUST be camelCase, ASCII, with the
first character a letter / `_` / `$`.

**Source:** Borrowed directly from the Google JSON Style Guide.

> Property names must be camelCased, ASCII strings.
> The first character must be a letter, an underscore (_) or a dollar sign ($).

Reference: https://google.github.io/styleguide/jsoncstyleguide.xml

Rationale: single predictable casing across Validator, Diff Engine, and future CLI /
MCP Server output; avoids snake_case vs camelCase drift seen in the pre-contract
ad hoc `toJson()` output.

## 2. Optional / absent fields: omit, don't null

**Rule:** If a property is optional, empty, or null, OMIT the key entirely rather than
emitting an explicit `null` — unless there is a strong semantic reason for its
existence (e.g. a three-state `true / false / unknown` where `null` means
"unknown" and absence would be ambiguous; such cases must be documented per-field
in the payload schema).

**Source:** Borrowed directly from the Google JSON Style Guide.

> If a property is optional or has an empty or null value, consider dropping the
> property from the JSON, unless there's a strong semantic reason for its existence.

Reference: https://google.github.io/styleguide/jsoncstyleguide.xml

Rationale: smaller payloads, cleaner diffs, and no ambiguity between "absent" and
"explicitly null" for consumers.

## 3. Forward compatibility: consumers must ignore unknown fields

**Rule:** Any code reading ParseX's JSON output (including ParseX's own future code
reading its own output back in) MUST NOT fail on unrecognized object keys or
unrecognized enum string values — it must ignore them and continue with the known
subset. Producers MUST NOT rely on consumers rejecting unknown fields.

**Source:** ParseX-specific extension, NOT from the Google JSON Style Guide.

We checked the Google JSON Style Guide specifically for a stated unknown-field rule
and it does not contain one — so this rule is not attributed to that guide.
Instead it is modeled on a real, documented compatibility mechanism from the
Language Server Protocol specification:

> The using side of an enumeration shouldn't fail on an enumeration value it doesn't know.
> ... it should simply ignore it.

Reference: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/

Here that enum-value principle is generalized from enumeration values to unknown
JSON object keys as a ParseX-specific extension (not a literal quote about JSON
object keys). The intent is identical: a future minor version adding an optional
field or a new `kind` value must not break already-deployed readers.

Implementation note: ParseX-side JSON parsing uses nlohmann/json's default lenient
key access (`.contains()` / `.value()` with defaults, never a strict closed-schema
parse), so extra keys are naturally ignored. See `CONVENTIONS.md` §3 tests in
`tests/libparsex/json_contract/` for the automated tolerance check.

## How to use this document

1. When adding a new `kind` payload schema (or changing an existing one), check each
   new/renamed key against §1 (camelCase regex) and §2 (omit-if-optional).
2. When adding parsing code that reads ParseX's own JSON back in, add a case with
   one extra unknown key and confirm it does not throw (§3).
3. When in doubt about sourcing, keep the distinction above: §1–§2 are Google-guide
   quotes; §3 is LSP-inspired but ParseX-specific. Do not reattribute §3 to Google.
