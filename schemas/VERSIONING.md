# ParseX JSON Contract Versioning

`contractVersion` (see `schemas/envelope.schema.json`) uses plain
`major.minor.patch` semver. There is exactly one project-wide constant
(`parsex::json_contract::kContractVersion`); every JSON-emitting code path
references it.

## Scheme

* **major** — bump on any breaking change (per the table below).
* **minor** — bump on additive / backward-compatible changes: a new optional
  field, a new `kind` value, a new enum value.
* **patch** — documentation-only or non-schema-affecting fixes (typo in a
  description, example update, no validator-visible change).

ParseX starts at `contractVersion: "0.1.0"` — there is no prior public JSON
contract to be compatible with yet.

Conceptual model adapted from Stripe's API versioning (dated major releases with
compatible minor updates — additive changes don't bump the compatibility-breaking
counter). Adapted to plain semver since ParseX doesn't need dated release names,
just the underlying principle. Reference: https://docs.stripe.com/upgrades

## Breaking-change classification

Each row notes its real source — or an honest "ParseX extension" label where the
rule goes beyond the cited documents.

| Change | Breaking? | Source |
|--------|-----------|--------|
| Adding an optional property | Non-breaking | Creek Service, "Evolving JSON Schemas – Part II" |
| Removing an optional property | Non-breaking for producers, BREAKING for any consumer that depended on it — producer/consumer asymmetry is explicit, do not gloss over it | Creek Service (asymmetry called out per that article) |
| Adding a required property | Breaking (backward-incompatible) | Creek Service; Microsoft Azure REST API Guidelines |
| Removing a required property | Breaking (forward-incompatible) | Creek Service; Microsoft Azure REST API Guidelines |
| Changing a field's required/optional status, either direction (optional→required or required→optional) | Breaking | Microsoft Azure REST API Guidelines — "It is a breaking change to introduce required fields in a later version; in addition, it is a breaking change to remove a required field or make an optional field required or vice versa" |
| Removing a value from an enumeration, or narrowing an enum's allowed values | Breaking. Adding a new enum value is non-breaking | Microsoft Azure REST API Guidelines — "Removing a value from an enumeration list breaks customer code" |
| Changing a field's type (e.g. string→number, object→array) | Breaking | ParseX-specific extension of the same principle — NOT directly sourced from either cited document; included as uncontroversial but labeled as such rather than misattributed |

References:

* Creek Service, "Evolving JSON Schemas – Part II" (optional/required property
  compatibility rules):
  https://www.creekservice.org/articles/2024/01/09/json-schema-evolution-part-2.html
* Microsoft Azure REST API Guidelines (required-field and enum-removal quotes):
  https://github.com/microsoft/api-guidelines

## Research gap (deliberate, not overlooked)

Confluent Schema Registry's well-known BACKWARD / FORWARD / FULL compatibility-mode
taxonomy would have been a strong additional citation, but its documentation pages
were not fetchable as static content during research — so this policy does not cite
Confluent's specific taxonomy, only the two sources actually confirmed above. A future
contributor may revisit this if those pages become citable; until then the omission
is intentional.
