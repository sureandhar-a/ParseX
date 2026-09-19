# Shared Types

The shared-types module (`libparsex/include/parsex/model/`) holds the plain-data
structs everything else in ParseX passes around: the six CAN domain types and the
two containers that assemble them. All types are aggregates with
compiler-generated copy/move semantics — no custom constructors. Copying a
`ParsedFile` deep-copies its vectors but *shares* its `RawDocument` via
`shared_ptr` (see `tests/libparsex/model/move_copy_test.cpp`, which pins this
behavior down explicitly).

## CommonFields

Every domain type embeds a `common` member of type `CommonFields`
(`model/common_fields.hpp`): the required `shortName`, the optional `category`,
and the `rawSpanRef` locating the element in the source file. It is composed in,
not inherited — each struct holds a `CommonFields common;` member. The reasoning
(no virtual dispatch, no slicing, data-only relationship) lives in
[0001-composition-over-inheritance](decisions/0001-composition-over-inheritance.md)
and is not repeated here. In practice this just means you write
`frame.common.shortName` instead of `frame.shortName`.

## protocolSpecific

`ProtocolSpecific` (`model/protocol_specific.hpp`) is a
`std::variant<std::monostate, CanExtension>` — the escape hatch for
protocol-dependent data that doesn't belong in the shared structs.
`std::monostate` (the default) means "no protocol data";
`CanExtension` (`model/can_extension.hpp`) is currently an empty placeholder for
future CAN-specific fields.

To add a new protocol extension later: define the new struct, add it as a
variant alternative, then add an `if constexpr` branch for it at every
`std::visit` call site (see `protocol_specific_test.cpp` for the established
pattern). Prefer an exhaustive visitor — e.g. an `else { static_assert(...) }`
fallback — so any call site you miss fails at compile time instead of silently
ignoring the new alternative. That exhaustiveness is the whole reason for
`std::variant` over `std::any`.

## Where things live

All paths under `libparsex/include/parsex/model/`:

| File | Type | Purpose |
| --- | --- | --- |
| `cluster.hpp` | `Cluster` | A communication cluster (e.g. a CAN bus): baudrate plus physical channels. |
| `ecu_instance.hpp` | `EcuInstance` | One ECU: the channels it connects to and its controllers. |
| `frame.hpp` | `Frame` (+ `FramePduMapping`) | A transmittable frame: length, transmitters, and the PDUs packed into it. |
| `pdu.hpp` | `Pdu` (+ `PduSignalMapping`, `ByteOrder`) | A protocol data unit: length and the signals mapped into it. |
| `signal.hpp` | `Signal` (+ `ValueTableEntry`) | One signal: bit layout, scaling, limits, receivers, optional value table. |
| `signal_group.hpp` | `SignalGroup` | A named set of signals that travel together. |
| `parsed_file.hpp` | `ParsedFile` (+ `Warning`) | One parsed ARXML file: one vector per domain type, warnings, and the shared `RawDocument`. |
| `parsed_project.hpp` | `ParsedProject` (+ `ResolvedReference`) | The multi-file view: all `ParsedFile`s plus resolved cross-file references. |
| `common_fields.hpp` | `CommonFields` | The shared identity/location block composed into each domain type. |
| `protocol_specific.hpp` | `ProtocolSpecific` | The protocol-extension variant described above. |

Rule of thumb: editing per-element data → open the domain type's file; wiring
files together → `parsed_file.hpp` / `parsed_project.hpp`. The headers are the
field-level reference — they are small enough to read directly.
