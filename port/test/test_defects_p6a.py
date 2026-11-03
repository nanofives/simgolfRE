"""P6A (Mariano's play-test 2, 2026-11-03): Gary Golf walks off the land, and the camera follows him.

updateWalkers.cursorTargetOffGrid: Gary Golf's walker (kind 0xfa, -6) walks to the cursor tile while hole 1 has no par
(updateWalkers+0x6db..updateWalkers+0x727), screenToTile gives tiles past the grid over the black void, and pathStep returns
its unclamped direction when the clamped walker and target tiles meet (pathStep+0x2f, pathStep+0xdd..pathStep+0xf2). Default
1 = the original: Gary walks past x = 0xc7ff toward an off-grid cursor; 0: he stops on the edge tile.
mainLoop.cameraFollowOffGrid: checked through the 'centre on golfer' key path is not cheap here; its unit check is that the
default build leaves every digest unchanged (segments gate). Notes: log/roadmap/P6/P6A_notes.md."""
from __future__ import annotations

from pathlib import Path

import pytest

from port.test.test_golfers import VL

pytestmark = pytest.mark.port

SEGMENTS = Path(__file__).resolve().parents[1] / "corpus" / "segments"
WALKER = 0x4C


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    portlib.negctl_reset()
    yield
    portlib.defect_reset()
    portlib.negctl_reset()


def _off(lib, name):
    for n, o, _ in lib.objects():
        if n == name:
            return o
    raise KeyError(name)


def _run_gary(lib, switch, cursor, frames=400):
    p = SEGMENTS / "sandbox_basic" / "00" / "start.data.bin"
    if not p.exists():
        pytest.skip(f"no segment corpus: {p}")
    img = p.read_bytes()
    lib.defect("updateWalkers.cursorTargetOffGrid", switch)
    o_w, o_h = _off(lib, "g_walkers"), _off(lib, "g_holes")
    xs = []
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(0, img)
        for k in range(64):                                      # only walker 0 live
            st.write(o_w + k * WALKER + 0x12, b"\x00")
        w = o_w
        rec = bytearray(WALKER)
        rec[0:4] = ((44 << 10) + 0x200).to_bytes(4, "little")   # tile (44, 25)
        rec[4:8] = ((25 << 10) + 0x200).to_bytes(4, "little")
        rec[0x10] = 0xFF                                         # no post
        rec[0x12] = 1                                            # live
        rec[0x13] = 0xFA                                         # Gary Golf's kind
        st.write(w, bytes(rec))
        st.write(o_h + 0x208, b"\x00")                           # hole 1 par 0
        st.write(_off(lib, "g_playerGolfer"), (0xFFFFFFFF).to_bytes(4, "little"))
        st.write(_off(lib, "g_flags"), (0).to_bytes(4, "little"))
        st.write(_off(lib, "g_cursorTileX"), cursor[0].to_bytes(4, "little", signed=True))
        st.write(_off(lib, "g_cursorTileY"), cursor[1].to_bytes(4, "little", signed=True))
        for f in range(frames):
            st.write(_off(lib, "g_date"), (f * 4).to_bytes(4, "little"))   # bit 0x200 clear for f < 128
            if f >= 128:
                st.write(_off(lib, "g_date"), ((f - 128) * 4).to_bytes(4, "little"))
            st.write(w + 0x1A, b"\x00\x00")                      # no idle wait between steps
            st.call("updateWalkers", [0])
            xs.append(int.from_bytes(st.read(w, 4), "little", signed=True))
    return xs


def test_original_walks_off_the_grid(portlib):
    xs = _run_gary(portlib, 1, (60, 25))
    assert max(xs) > 0xC7FF, f"the original walks past the edge toward the off-grid cursor (max x {max(xs):#x})"


def test_fixed_stops_on_the_edge(portlib):
    xs = _run_gary(portlib, 0, (60, 25))
    assert max(xs) <= 0xC7FF
    assert max(xs) >= (48 << 10) + 0x200, "he still walks to within the arrival distance 0x400 of the edge tile centre"


def test_in_grid_cursor_unchanged(portlib):
    assert _run_gary(portlib, 1, (30, 25)) == _run_gary(portlib, 0, (30, 25))
