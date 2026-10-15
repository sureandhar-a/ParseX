#!/usr/bin/env python3
"""Cross-check a ParseX DBC export against cantools reading the same ARXML.

cantools (https://github.com/cantools/cantools) is an independent ARXML and
DBC implementation, so agreement between "ARXML -> cantools" and
"ARXML -> parsex export -> DBC -> cantools" is strong evidence the export is
right. Every frame and signal ParseX exports must match cantools on id,
extended flag, length, start bit, bit length, byte order, signedness,
scale, offset, unit and value table. Frames cantools knows but ParseX
skips (multiplexed / container PDUs, out of scope) are listed, not failed.

ponytail: cantools ignores the PDU offset inside a frame
(PDU-TO-FRAME-MAPPING START-POSITION); fixtures with non-zero offsets would
show start-bit mismatches that are cantools' gap, not ours.

Usage:
    pip install cantools
    parsex export -i system.arxml -f dbc -o system.dbc
    python3 scripts/check_dbc_against_cantools.py system.arxml system.dbc
Exit 0 when everything ParseX exported matches, 1 otherwise.
"""

import math
import sys

import cantools


def signal_fields(sig):
    choices = {int(k): str(v) for k, v in (sig.choices or {}).items()}
    return {
        "start": sig.start,
        "length": sig.length,
        "byte_order": sig.byte_order,
        "is_signed": sig.is_signed,
        "scale": float(sig.scale),
        "offset": float(sig.offset),
        "unit": sig.unit or "",
        "choices": choices,
    }


def same(left, right):
    if isinstance(left, float) and isinstance(right, float):
        return math.isclose(left, right, rel_tol=1e-9, abs_tol=1e-12)
    return left == right


def main(arxml_path, dbc_path):
    reference = cantools.database.load_file(arxml_path, strict=False)
    exported = cantools.database.load_file(dbc_path, strict=False)
    ref_by_name = {m.name: m for m in reference.messages}
    failures = []
    checked_signals = 0

    for msg in exported.messages:
        ref = ref_by_name.get(msg.name)
        if ref is None:
            failures.append(f"frame {msg.name}: not in cantools' ARXML view")
            continue
        for field in ("frame_id", "is_extended_frame", "length", "is_fd"):
            if getattr(msg, field) != getattr(ref, field):
                failures.append(f"frame {msg.name}.{field}: "
                                f"{getattr(msg, field)!r} != {getattr(ref, field)!r}")
        ref_signals = {s.name: s for s in ref.signals}
        for sig in msg.signals:
            ref_sig = ref_signals.get(sig.name)
            if ref_sig is None:
                failures.append(f"signal {msg.name}.{sig.name}: not in cantools' view")
                continue
            ours, theirs = signal_fields(sig), signal_fields(ref_sig)
            for key, value in ours.items():
                if not same(value, theirs[key]):
                    failures.append(f"signal {msg.name}.{sig.name}.{key}: "
                                    f"{value!r} != {theirs[key]!r}")
            checked_signals += 1

    exported_names = {m.name for m in exported.messages}
    skipped = sorted(set(ref_by_name) - exported_names)
    print(f"checked {len(exported.messages)} frames, {checked_signals} signals")
    if skipped:
        print(f"not exported by ParseX (out of scope): {', '.join(skipped)}")
    for failure in failures:
        print(f"MISMATCH {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1], sys.argv[2]))
