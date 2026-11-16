"""P32D fix 10 (play-test 14 item 10): loadGolfPro.fieldWalksOnly. loadGolfPro builds the tournament field with every record
zeroed, and the may-ride flag (golfer flags bit 16) that members get with a connected Cart Garage is never set for it. At 0 the
field (the pro, golfer 0, and the player's golfer 1) gets bit 16 when g_activeObjectLevel[11] != 0; at 1, or without a garage,
the field's flags are those of the original. log/roadmap/P32/P32D_notes.md."""
from __future__ import annotations

import pathlib
import struct
import sys

import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
GOLFER = 0x100
MAY_RIDE = 1 << 16


def _field_flags(portlib, switch: int, garage: int) -> tuple[list[int], bytes, bytes]:
    o = {n: off for n, off, _ in portlib.objects()}
    try:
        with portlib.state() as st:
            st.set_data_base(VL.DATA_VA)
            st.write(o["g_holes"] + 0x208, b"\x03")                         # hole 1 open (par 3): golfers 0 and 1
            st.write(o["g_activeObjectLevel"] + 11 * 4, struct.pack("<i", garage))
            portlib.defect("loadGolfPro.fieldWalksOnly", switch)
            try:
                st.call("loadGolfPro", [0])
            except L.SgError as e:
                if e.code == L.SG_E_NO_RESOURCE:
                    pytest.skip(f"loadGolfPro needs a resource this context lacks: {e}")
                raise
            flags = [struct.unpack("<I", st.read(o["g_golfers"] + g * GOLFER + 0x10, 4))[0] for g in range(2)]
            rng = st.read(o["g_rng"], 4)
            img = st.image()
    finally:
        portlib.defect("loadGolfPro.fieldWalksOnly", 1)
    return flags, rng, img


@pytest.mark.parametrize("garage", [0, 1, 2])
@pytest.mark.parametrize("switch", [1, 0])
def test_field_may_ride(portlib, switch, garage):
    flags, _, _ = _field_flags(portlib, switch, garage)
    ride = switch == 0 and garage != 0
    for g in range(2):
        assert bool(flags[g] & MAY_RIDE) == ride, (g, hex(flags[g]))


def test_field_switch_changes_only_the_flag(portlib):
    """With a garage, 0 against 1: the same random draws and the same state except bit 16 of the two field records' flags."""
    f1, rng1, img1 = _field_flags(portlib, 1, 1)
    f0, rng0, img0 = _field_flags(portlib, 0, 1)
    assert rng0 == rng1
    assert [f ^ MAY_RIDE for f in f0] == f1
    diff = [i for i in range(len(img0)) if img0[i] != img1[i]]
    o = {n: off for n, off, _ in portlib.objects()}
    assert diff == [o["g_golfers"] + g * GOLFER + 0x12 for g in range(2)]
