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


# ------------------------------------------------------------------------------------------------ P36B fix 4 (Mariano's picks A, B)
OVERLAP = "updateGolfers.partnerWaitsBallRest"
GATE = "updateGolfers.preShotGateByHole"


def _world(lib, st, keep_n):
    """mood5_fixture/00 loaded with only its first keep_n live golfers on the course (every other hole byte 0), outside a championship,
    no player's golfer, no shot chosen this frame. Returns the objects map."""
    from port.test.test_golfers import VL
    o = _objs(lib)
    st.set_data_base(VL.DATA_VA)
    st.write(0, _seg_image("mood5_fixture", "00"))
    G = o["g_golfers"]
    live = [g for g in range(0x98) if st.read(G + g * 0x100 + 0x21, 1)[0] not in (0, 0xff)]
    assert len(live) >= 2, live
    for g in range(0x98):
        if g not in live[:keep_n]:
            st.write(G + g * 0x100 + 0x21, b"\x00")
    fl = struct.unpack("<I", st.read(o["g_flags"], 4))[0]
    st.write(o["g_flags"], struct.pack("<I", fl & ~0x4200000))
    st.write(o["g_playerGolfer"], struct.pack("<i", -1))
    st.write(o["unk_0059b738"], struct.pack("<i", 0))
    st.write(o["unk_0059b04c"], struct.pack("<i", 0))
    return o, live


def _pair_shot(lib, switch, champ=False):
    """golfer i (played: flag 0x400, ball at rest, ready +0x20 = 1) whose partner p has not played on the hole and has its ball in
    the air (flag 0x40000, speed 0x400). Returns i's ball speed +0xe4 after one updateGolfers call (non-zero = i took its shot)."""
    lib.defect(OVERLAP, switch)
    with lib.state() as st:
        o, live = _world(lib, st, 2)
        if champ:   # a championship or pro match (g_flags 0x200000 of 0x4200000): the turn order stays the original's
            fl = struct.unpack("<I", st.read(o["g_flags"], 4))[0]
            st.write(o["g_flags"], struct.pack("<I", fl | 0x200000))
        i, p = live[0], live[1]
        G = o["g_golfers"]
        ri, rp = G + i * 0x100, G + p * 0x100
        hole = st.read(ri + 0x21, 1)
        for r, other in ((ri, p), (rp, i)):
            st.write(r + 0x21, hole)
            st.write(r + 0xa2, struct.pack("<h", other))
            x, y = struct.unpack("<ii", st.read(r, 8))
            st.write(r + 0xd4, struct.pack("<iiiiiiI", x, y, 0, 0, 0, 0, 0))   # ball at the golfer's feet, at rest
            st.write(r + 0x9e, struct.pack("<h", 0))
        st.write(ri + 0x10, struct.pack("<I", 0x400))
        st.write(ri + 0x1d, b"\x0b")
        st.write(ri + 0x20, b"\x01")
        st.write(ri + 0x22, b"\x01")
        st.write(rp + 0x10, struct.pack("<I", 0x40000))
        st.write(rp + 0x1d, b"\x0b")
        st.write(rp + 0x22, b"\x00")
        st.write(rp + 0xe4, struct.pack("<i", 0x400))
        st.write(rp + 0xdc, struct.pack("<ii", 0x100, 0x40))
        st.call("updateGolfers", [0])
        return struct.unpack("<i", st.read(ri + 0xe4, 4))[0]


def _gate(lib, switch, hole, seeds):
    """one golfer alone, ready at its ball (+0x20 = 1, ball speed set), off the green: how many of the rng seeds let it into the
    swing (state 0x10) in one updateGolfers call (the pre-shot gate at updateGolfers+0x373a)"""
    lib.defect(GATE, switch)
    n = 0
    for seed in seeds:
        with lib.state() as st:
            o, live = _world(lib, st, 1)
            i = live[0]
            G = o["g_golfers"]
            r = G + i * 0x100
            st.write(r + 0x21, bytes([hole, 1]))                 # its hole, 1 stroke
            st.write(r + 0x10, struct.pack("<I", 0))
            st.write(r + 0x1d, b"\x0b")
            st.write(r + 0x20, b"\x01")
            st.write(r + 0x9e, struct.pack("<hh", 0, i))
            x, y = struct.unpack("<ii", st.read(r, 8))
            st.write(r + 0xd4, struct.pack("<ii", x, y))
            st.write(r + 0xe4, struct.pack("<i", 0x100))
            st.write(o["g_tileType"] + (y >> 10) + (x >> 10) * 50, b"\x02")   # the ball's tile: fairway (not a green)
            st.write(o["g_rng"], struct.pack("<I", seed))
            st.call("updateGolfers", [0])
            n += st.read(r + 0x1d, 1)[0] == 0x10
    return n


def test_fix4_switches_registered_default_on(portlib):
    d = portlib.defects()
    for s in (OVERLAP, GATE):
        assert s in d and d[s]["on"] == 1, s


def test_overlap_partner_ball_in_air(portlib):
    held, played = _pair_shot(portlib, 1), _pair_shot(portlib, 0)
    print(f"ball speed after the call: switch 1 {held:#x}, switch 0 {played:#x}")
    assert held == 0, "at 1 the golfer waits for its partner's ball to come to rest (the original)"
    assert played != 0, "at 0 the golfer takes its shot while the partner's ball flies"


def test_gate_hole1_draw(portlib):
    seeds = [0x1000 + 977 * k for k in range(450)]
    orig, fixed = _gate(portlib, 1, 5, seeds), _gate(portlib, 0, 5, seeds)
    h1 = _gate(portlib, 1, 1, seeds)
    print(f"hole 5: switch 1 {orig}, switch 0 {fixed}; hole 1 at 1: {h1} of {len(seeds)}")
    assert orig > 0
    assert fixed > 1.4 * orig, (orig, fixed)        # range(9) at 1 against range(5) at 0
    assert fixed == h1                              # at 0 hole 5 draws as hole 1 does (the same seeds)


def test_overlap_not_in_championship(portlib):
    assert _pair_shot(portlib, 0, champ=True) == 0, "in a championship or pro match the golfer still waits at 0"
