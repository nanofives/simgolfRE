"""P36B fix 1 (play switch updateGolfers.playerPairBlocksWalkers, log/roadmap/P36/P36B_notes.md section 5).

The walking step's in-the-way test (updateGolfers, the loop at updateGolfers+0x2672..updateGolfers+0x288c) stops a golfer that has
another golfer within half a tile in its walking direction. With the switch at 1 (the original) the player's golfer standing with
its shot handed over (g_playerGolfer, flag 0x200, target x +0xcc = -1) stops a walker like any golfer; at 0 the walker steps
through it. Setting: mood5_fixture/00, a free record made the player's golfer a quarter tile ahead of a walking golfer, on its
hole, with the same arrival order and a long wait (so its own tick changes nothing). One updateGolfers call per case.
Run as one file per process: `pytest port/test/test_golfers_p36b.py`.
"""
from __future__ import annotations

import struct

import pytest

from port.test.test_defects_p28 import _seg_image

pytestmark = pytest.mark.port

SWITCH = "updateGolfers.playerPairBlocksWalkers"
DX = (0, 1, 1, 1, 0, -1, -1, -1)   # f_tab_dirDx_4c2878 / f_tab_dirDy_4c2898 (direction8's octants)
DY = (-1, -1, 0, 1, 1, 1, 0, -1)


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def _objs(lib):
    return {n: v for n, v, _ in lib.objects()}


def _walkers(img_rec):
    """golfers of the image on a hole, walking (state 7..10), not waiting, none of the loop's skip flags 0x21002000"""
    out = []
    for g in range(0x98):
        r = img_rec(g)
        hole, state = struct.unpack_from("<bb", r, 0x21)[0], struct.unpack_from("<b", r, 0x1d)[0]
        flags = struct.unpack_from("<I", r, 0x10)[0]
        wait = struct.unpack_from("<h", r, 0x9e)[0]
        if 1 <= hole <= 18 and 7 <= state <= 10 and wait >= 0 and flags & 0x21002000 == 0:
            out.append(g)
    return out


def _step(lib, switch, b, a):
    """b's position before and after one updateGolfers call with record a made the player's waiting golfer ahead of b"""
    from port.test.test_golfers import VL
    img = _seg_image("mood5_fixture", "00")
    lib.defect(SWITCH, switch)
    o = _objs(lib)
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(0, img)
        G = o["g_golfers"]
        rb, ra = G + b * 0x100, G + a * 0x100
        x, y = struct.unpack("<ii", st.read(rb, 8))
        facing = st.read(rb + 0x1a, 1)[0] & 7
        hole = st.read(rb + 0x21, 1)
        order = st.read(rb + 0xbe, 2)
        st.write(ra, struct.pack("<ii", x + DX[facing] * 0x100, y + DY[facing] * 0x100))
        st.write(ra + 0x10, struct.pack("<I", 0x200))
        st.write(ra + 0x21, hole + b"\x00")                    # its hole, 0 strokes
        st.write(ra + 0x9e, struct.pack("<hh", -99, a))        # waits (its own tick does nothing), partner = itself
        st.write(ra + 0xbe, order)
        st.write(ra + 0xcc, struct.pack("<ii", -1, 0))         # the shot handed to the player
        st.write(ra + 0xd4, struct.pack("<i", 0))
        st.write(o["g_playerGolfer"], struct.pack("<i", a))
        fl = struct.unpack("<I", st.read(o["g_flags"], 4))[0]
        st.write(o["g_flags"], struct.pack("<I", fl & ~0x4200000))   # outside a championship / pro match
        st.call("updateGolfers", [0])
        x2, y2 = struct.unpack("<ii", st.read(rb, 8))
        wait = struct.unpack("<h", st.read(rb + 0x9e, 2))[0]
        return (x, y), (x2, y2), wait


def test_switch_registered_default_on(portlib):
    d = portlib.defects()
    assert SWITCH in d and d[SWITCH]["on"] == 1


def test_walker_passes_player_pair(portlib):
    from port.test.test_golfers import VL
    img = _seg_image("mood5_fixture", "00")
    G = _objs(portlib)["g_golfers"]
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(0, img)
        recs = [st.read(G + g * 0x100, 0x100) for g in range(0x98)]
    free = [g for g in range(0x98) if recs[g][0x21] == 0]
    walkers = _walkers(lambda g: recs[g])
    assert free and walkers, (len(free), len(walkers))
    a = free[-1]
    stopped = passed = 0
    for b in walkers:
        p0, p1, w1 = _step(portlib, 1, b, a)
        q0, q1, w0 = _step(portlib, 0, b, a)
        if p1 == p0 and w1 < 0:          # at 1 the waiting golfer stopped b (the original)
            stopped += 1
            if q1 != q0 and w0 >= 0:     # at 0 b stepped on
                passed += 1
    print(f"walkers {len(walkers)} stopped {stopped} passed {passed}")
    assert stopped > 0, "no walker reached the in-the-way test"
    assert passed == stopped, (passed, stopped)
