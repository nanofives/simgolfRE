"""FB14 (play-test feedback 1 item 14, play switch updateGolfers.teeWaitOffLand, log/roadmap/P41/feedback1/item14_notes.md).

A golfer with no shot yet walks to its hole's tee point. With a golfer ahead on the tee it walks one tile further back
(updateGolfers+0x17a7..updateGolfers+0x17e3), and the queue offsets put it half a tile beside or behind its partner
(updateGolfers+0x1d93..updateGolfers+0x1db5, updateGolfers+0x1e05..updateGolfers+0x1e27). None of them tests the land, so on a tee
at the land's edge the queue stands off the land (Mariano's Dolphin Coast, hole 5). At 1 (the original) that is kept; at 0 the
waiting spot and the offsets are used only when canStep allows the step into their tile, otherwise the golfer waits beside the tee
or on the tee point.

Setting: mood5_fixture/00 with four golfers left on hole 1: pair A held on the tee point (a long wait, so it never tees off) and
pair B, arrived later, standing on the side of the tee. Every tile behind the tee (the half-plane away from the cup, up to 3 tiles)
is made type 0x14, the type clearTile gives land outside the course. updateGolfers runs for a few hundred frames; the test counts
the frames in which a golfer of pair B stands on a 0x14 tile.
Run as one file per process: `pytest port/test/test_golfers_teewait.py`.
"""
from __future__ import annotations

import struct

import pytest

from port.test.test_defects_p28 import _seg_image

pytestmark = pytest.mark.port

SWITCH = "updateGolfers.teeWaitOffLand"
DX = (0, 1, 1, 1, 0, -1, -1, -1)   # f_tab_dirDx_4c2878 / f_tab_dirDy_4c2898
DY = (-1, -1, 0, 1, 1, 1, 0, -1)
OFF_LAND = 0x14
HOLE = 1
FRAMES = 600


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def _objs(lib):
    return {n: v for n, v, _ in lib.objects()}


def _run(lib, switch, frames=FRAMES):
    """pair B's positions per frame and the strokes of every golfer at the end"""
    from port.test.test_golfers import VL
    lib.defect(SWITCH, switch)
    o = _objs(lib)
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(0, _seg_image("mood5_fixture", "00"))
        G, H, T = o["g_golfers"], o["g_holes"], o["g_tileType"]
        live = [g for g in range(0x98) if st.read(G + g * 0x100 + 0x21, 1)[0] not in (0, 0xFF)]
        assert len(live) >= 4, live
        a0, a1, b0, b1 = sorted(live[:4])
        for g in range(0x98):
            if g not in (a0, a1, b0, b1):
                st.write(G + g * 0x100 + 0x21, b"\x00")
        fl = struct.unpack("<I", st.read(o["g_flags"], 4))[0]
        st.write(o["g_flags"], struct.pack("<I", fl & ~0x4200000))            # outside a championship / pro match
        st.write(o["g_playerGolfer"], struct.pack("<i", -1))
        st.write(o["g_nextArrivalOrder"], struct.pack("<i", 10))
        hr = H + HOLE * 0x208
        hdir = st.read(hr + 1, 1)[0] & 7
        tx, ty = struct.unpack("<ii", st.read(hr + 8, 8))
        st.write(hr + 0x10, struct.pack("<ii", tx, ty))                      # the second tee = the first
        # behind the tee: every tile within 3 whose offset from the tee points away from the cup side (dot with D(hdir) < 0)
        cleared = []
        for ddx in range(-3, 4):
            for ddy in range(-3, 4):
                if ddx * DX[hdir] + ddy * DY[hdir] < 0:
                    x, y = tx + ddx, ty + ddy
                    if 0 <= x < 50 and 0 <= y < 50:
                        st.write(T + x * 50 + y, bytes([OFF_LAND]))
                        cleared.append((x, y))
        # the tee point (updateGolfers+0x1730..updateGolfers+0x17a1): the tee tile's centre 0x140 (0xc0 diagonally) from the cup side
        bx, by = (tx << 10) + 0x200, (ty << 10) + 0x200
        cx, cy = struct.unpack("<ii", st.read(hr + 0x18, 8))
        m = (3 if hdir & 1 else 5) << 6
        px, py = bx - DX[hdir] * m, by - DY[hdir] * m
        side = (hdir + 2) & 7

        def golfer(g, partner, order, x, y, wait, ball):
            r = G + g * 0x100
            st.write(r, struct.pack("<ii", x, y))
            st.write(r + 0x10, struct.pack("<I", 0))
            st.write(r + 0x14, struct.pack("<h", 0))
            st.write(r + 0x18, b"\x00\x00")                                  # kind 0, kind flags 0 (no drinks / restrooms)
            st.write(r + 0x1d, b"\x0b")
            st.write(r + 0x20, bytes([0, HOLE, 0]))                           # hole, 0 strokes
            st.write(r + 0x84, b"\x01")
            st.write(r + 0x9e, struct.pack("<h", wait))
            st.write(r + 0xa2, struct.pack("<hhhhh", partner, 100, 0, 0, 0))   # partner, mood, not hungry / thirsty / tired
            st.write(r + 0xbe, struct.pack("<h", order))
            st.write(r + 0xd4, struct.pack("<ii", *ball))
            st.write(r + 0xe4, struct.pack("<i", 0))
        golfer(a0, a1, 1, px, py, -30000, (px, py))
        golfer(a1, a0, 1, px + 0x100, py, -30000, (px, py))
        sx, sy = (tx + DX[side]) << 10, (ty + DY[side]) << 10
        golfer(b0, b1, 2, sx + 0x200, sy + 0x200, 0, (0, 0))
        golfer(b1, b0, 2, sx + 0x280, sy + 0x200, 0, (0, 0))
        date = struct.unpack("<i", st.read(o["g_date"], 4))[0]
        pos = []
        for f in range(frames):
            st.write(o["g_date"], struct.pack("<i", date + f))
            st.call("updateGolfers", [0])
            pos.append([struct.unpack("<ii", st.read(G + g * 0x100, 8)) for g in (b0, b1)])
        types = st.read(T, 2500)
        holes = [st.read(G + g * 0x100 + 0x21, 1)[0] for g in (a0, a1, b0, b1)]
    off = [f for f, ps in enumerate(pos)
           if any(not (0 <= x >> 10 < 50 and 0 <= y >> 10 < 50) or types[(x >> 10) * 50 + (y >> 10)] == OFF_LAND for x, y in ps)]
    return off, pos, holes, cleared, (px, py)


def test_switch_registered_default_on(portlib):
    d = portlib.defects()
    assert SWITCH in d and d[SWITCH]["on"] == 1 and d[SWITCH]["ref"] == "model_defects#55"


def test_original_queues_off_the_land(portlib):
    off, pos, holes, cleared, tee = _run(portlib, 1)
    print(f"switch 1: {len(off)} of {FRAMES} frames with a pair-B golfer off the land; last {pos[-1]}, tee point {tee}, holes {holes}")
    assert holes[2] == HOLE and holes[3] == HOLE, "pair B is still on the hole"
    assert len(off) > FRAMES // 4, "at 1 the queue waits behind the tee, over the cleared tiles (the original)"


def test_fixed_queue_stays_on_the_land(portlib):
    off, pos, holes, cleared, tee = _run(portlib, 0)
    print(f"switch 0: {len(off)} of {FRAMES} frames off the land; last {pos[-1]}, tee point {tee}, holes {holes}")
    assert holes[2] == HOLE and holes[3] == HOLE
    assert off == [], f"at 0 no pair-B golfer stands off the land (first frames {off[:5]})"
    # it still walks to the tee and waits there: within 1.5 tiles of the tee point at the end
    for x, y in pos[-1]:
        assert max(abs(x - tee[0]), abs(y - tee[1])) < 0x600, (x, y, tee)
