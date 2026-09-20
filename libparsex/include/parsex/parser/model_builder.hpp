#pragma once

#include <parsex/model/cluster.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>
#include <parsex/raw/raw_node.hpp>

// Protocol Model Builder: one small function per domain type, each projecting
// a single RawNode (plus its descendants — never other elements) into its
// typed object. No cross-element joins here: anything that lives on another
// element (I-SIGNAL packing on the owning PDU's mapping, COMPU-METHOD scaling
// behind a ref, frame transmitters behind triggerings) is resolved at project
// level later; single-node fields below fall back to domain defaults.
//
// Field contract (uniform): absent-or-unparseable XML keeps the domain
// default (best-effort parsing — hard conformance is the Validator's job).
// Reference-valued strings (TRANSMITTER-REF, PDU-REF, *-REF, ...) are reduced
// to short names (text after the last '/'): the domain's join keys are short
// names, and the full path is not recoverable from a short name while the
// reverse always is.
//
// Observed XML shapes (per-release notes — read from the fixtures, not assumed):
//
// tiny_valid.arxml (simplified study vocabulary):
//   CLUSTER{SHORT-NAME, CATEGORY?, BAUDRATE, PHYSICAL-CHANNELS/PHYSICAL-CHANNEL/SHORT-NAME}
//   ECU-INSTANCE{SHORT-NAME, CATEGORY?, CONNECTED-CHANNELS/CHANNEL-REF,
//     CONTROLLERS/CAN-CONTROLLER/SHORT-NAME}
//   FRAME{SHORT-NAME, LENGTH, TRANSMITTERS/TRANSMITTER-REF,
//     PDUS/FRAME-PDU{SHORT-NAME, PDU-REF, START-POSITION}}
//   PDU{SHORT-NAME, LENGTH,
//     SIGNAL-MAPPINGS/PDU-SIGNAL-MAPPING{SHORT-NAME, SIGNAL-REF, START-POSITION, BYTE-ORDER}}
//   SYSTEM-SIGNAL{SHORT-NAME, START-BIT, BIT-LENGTH, BYTE-ORDER,
//     VALUE-TABLE/VALUE-TABLE-ENTRY{VALUE, LABEL}, RECEIVERS/RECEIVER-REF}
//   SIGNAL-GROUP{SHORT-NAME, MEMBERS/SYSTEM-SIGNAL-REF}
//
// system-4.2.arxml (real 4.2-era CAN vocabulary; AUTOSAR_00046.xsd):
//   CAN-CLUSTER{SHORT-NAME,
//     CAN-CLUSTER-VARIANTS/CAN-CLUSTER-CONDITIONAL{BAUDRATE,
//       PHYSICAL-CHANNELS/CAN-PHYSICAL-CHANNEL/SHORT-NAME}}
//   ECU-INSTANCE{SHORT-NAME, ASSOCIATED-COM-I-PDU-GROUP-REFS/... (not channels),
//     COMM-CONTROLLERS/CAN-COMMUNICATION-CONTROLLER/SHORT-NAME}
//   CAN-FRAME{SHORT-NAME, FRAME-LENGTH,
//     PDU-TO-FRAME-MAPPINGS/PDU-TO-FRAME-MAPPING{PDU-REF, START-POSITION}}
//     (no TRANSMITTERS on the node — transmitters live behind FRAME-TRIGGERINGs)
//   I-SIGNAL-I-PDU{SHORT-NAME, LENGTH,
//     I-SIGNAL-TO-PDU-MAPPINGS/I-SIGNAL-TO-I-PDU-MAPPING{I-SIGNAL-REF,
//       START-POSITION, PACKING-BYTE-ORDER?}}
//   I-SIGNAL{SHORT-NAME, LENGTH, INIT-VALUE/NUMERICAL-VALUE-SPECIFICATION/VALUE,
//     NETWORK-REPRESENTATION-PROPS/.../BASE-TYPE-REF, SYSTEM-SIGNAL-REF}
//     (no START-BIT / BYTE-ORDER / RECEIVERS on the node — packing lives on the
//     owning PDU's mapping and lands in PduSignalMapping, not here)
//   SYSTEM-SIGNAL{SHORT-NAME, PHYSICAL-PROPS/.../COMPU-METHOD-REF}
//     (scaling behind the COMPU-METHOD ref — factor/offset stay default here)
//   I-SIGNAL-GROUP{SHORT-NAME, I-SIGNAL-REFS/I-SIGNAL-REF,
//     TRANSFORMATION-... (ignored)}
//
// R21-11 shapes are UNVERIFIED (no R21-11 communication fixture on hand): the
// readers below accept both vocabularies structurally (LENGTH or FRAME-LENGTH,
// either mapping tag, either channel tag), so identical vocabulary parses;
// genuinely new R21-11 nesting needs a fixture-driven revisit.
//
// Known approximations (domain-fidelity gaps, not bugs):
// - ByteOrder has only First-variants: MOST-SIGNIFICANT-BYTE-LAST and friends
//   fall back to MostSignificantByteFirst (the domain default).
// - I-SIGNAL signedness is heuristical: BASE-TYPE-REF's short name matching
//   S(INT|[0-9]) (S16, SINT8, ...) counts as signed; the authoritative
//   BASE-TYPE-ENCODING lives on the referenced SW-BASE-TYPE element, which a
//   single-node build cannot reach.
// - Same-short-name collisions across packages would confuse short-name joins;
//   accepted v1 limitation (Resolver revisit).

Cluster buildCluster(const RawNode& node);
EcuInstance buildEcuInstance(const RawNode& node);
Frame buildFrame(const RawNode& node);
Pdu buildPdu(const RawNode& node);
Signal buildSignal(const RawNode& node);
SignalGroup buildSignalGroup(const RawNode& node);
