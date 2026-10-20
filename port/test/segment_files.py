"""Extra game files of the install a segment's recording ran against (phase 3.8, stream 38b).

A recording saw the directory of its instance: the pristine saved games plus whatever the scenario copied in. The browser
(loadGameBrowserScreen) lists "Saved Games\\*.sve", so a segment that reaches it needs the same list. SegmentRunner attaches
`files_for(scenario)` on top of the install (the context's in-memory overlay: original\\ is never written).

cov_a_flags (re/tools/scenario.py: "List rows: 0, 1 autosaves, 2 mood5, 3 stories5, 4 stories9, 5 zdemo, 6 zedit, 7 ztut"):
log/c3/c3fya_run.py copied three fixture saves into the instance: mood5.sve with one bit added to the flag word [g_flags]
(file offset 0x44e09, a dword; log/c3/c3fya/fixtures/, game-derived, not committed): ztut.sve 0x8000, zdemo.sve 0x8000000,
zedit.sve 0x2000000. They are derived here from the install's mood5.sve (the committed fixtures, when present, must equal
the derivation: tests/test_segment_files.py).
"""
from __future__ import annotations

import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[2]
INSTALL = ROOT / "original"
FLAGS_AT = 0x44E09        # the g_flags dword of a .sve (re/model/sve_fields.tsv index 70)
COV_A_FLAGS_FIXTURES = {"ztut.sve": 0x8000, "zdemo.sve": 0x8000000, "zedit.sve": 0x2000000}


def flagged_copy(raw: bytes, bit: int) -> bytes:
    b = bytearray(raw)
    (v,) = struct.unpack_from("<I", b, FLAGS_AT)
    struct.pack_into("<I", b, FLAGS_AT, v | bit)
    return bytes(b)


def files_for(scenario: str) -> dict:
    """{logical name: bytes} to add to the install for the scenario's segments ({} when it needs none)"""
    if scenario == "cov_a_flags":
        base = INSTALL / "Saved Games" / "mood5.sve"
        if not base.exists():
            return {}
        raw = base.read_bytes()
        return {"Saved Games/" + n: flagged_copy(raw, bit) for n, bit in COV_A_FLAGS_FIXTURES.items()}
    return {}
