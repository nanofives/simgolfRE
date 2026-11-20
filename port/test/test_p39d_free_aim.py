"""P39D (re/roadmap/P39_COURSE_FEATURES.md "P39D", log/roadmap/P39/P39D_notes.md): free shot aiming with a snap to the cup, play
switch mainLoop.aimHalfTileLattice (1 = the original's half-tile lattice and tile-centre shot).

- aim_free_point (aimFree.point) solves the world point under a screen point: from worldToScreen of a random world point it gives
  back a point whose exact projection is within 0.5 pixel of the requested one, flat and sloped, started up to two tiles off.
- The snap: within aim_free_snap_radius (max(0x200, 0x800 / zoom)) of the cup centre and on the hole's own green the point is the
  cup centre; outside the radius, or on a tile off the green, it is not.
- The player's real shot inside a FreeTargetScope (prepareShot.free) towards the cup tile's centre leaves the same state, byte for
  byte, as the original's prepareShot(g, 1, -1, 0, 0) at that target tile (the snap equals the cup-tile shot); an off-centre free
  point inside the tile changes the shot's direction (+0xe0) and keeps the target tile.
- Frame runs of corpus segments that start with the player aiming: with the switch at 1 the run reproduces the recording and holds
  no free point; at 0 the aim block holds a free point for the player's golfer whose tile is the aim tile +0xc4 / +0xc8.
Negative control: an aim solver that stops after the first step (a test-side bound of 1 px on a deliberately far start) reads RED.
"""
from __future__ import annotations

import ctypes as C
import math
import random
import struct

import pytest

from port.test import lib as L
from port.test.test_terrain import need_exe, new_state, objs
from port.test.test_p39c_green_highlight import _state, _buf, _BUF, _w2s, _s32, _aim, OUT_VA, layout

pytestmark = pytest.mark.port

SW = "mainLoop.aimHalfTileLattice"


def _point(st, mx, my, cx, cy, hole):
    b = _buf(st)
    st.call("aimFree.point", [int(round(mx * 16)) & 0xFFFFFFFF, int(round(my * 16)) & 0xFFFFFFFF, cx, cy, hole, OUT_VA])
    return tuple(b[:8])   # px, py, snap, rx, ry, radius, exact x * 16, exact y * 16


def _slope(st, n=50):
    lv = bytearray(51 * 51)
    for x in range(51):
        for y in range(51):
            lv[x * 51 + y] = min(15, 3 + y // 2 + x // 4)
    st.mem_write(st.grid_va("g_cornerLevel"), bytes(lv))


def _set_cup(portlib, st, hole, tx, ty):
    o = objs(portlib)
    st.write(o["g_holes"][0] + 0x208 * hole + 0x18, struct.pack("<ii", tx, ty))


@need_exe
@pytest.mark.parametrize("sloped", [False, True])
def test_solver_inverts_world_to_screen(portlib, sloped):
    st, T, F = _state(portlib, 50)
    try:
        _aim(portlib, st, 1, (25, 25))
        if sloped:
            _slope(st)
        rng = random.Random(39)
        worst = 0.0
        for _ in range(200):
            wx, wy = rng.randrange(18 << 10, 32 << 10), rng.randrange(18 << 10, 32 << 10)
            sx, sy = _w2s(st, wx, wy)
            cx, cy = (wx >> 10) + rng.randrange(-2, 3), (wy >> 10) + rng.randrange(-2, 3)
            px, py, snap, rx, ry, rad, ex, ey = _point(st, sx, sy, cx, cy, 0)
            assert snap == 0 and (px, py) == (rx, ry)
            # the exact projection of the solved point against the requested screen point (the solver stops within 0.25 px; the
            # point's rounding to world units adds 1 / 32 px)
            worst = max(worst, math.hypot(ex / 16 - sx, ey / 16 - sy))
        print("solver worst residual px", worst, "sloped" if sloped else "flat")
        assert worst <= 0.5, worst
    finally:
        _BUF.pop(id(st), None)
        st.close()


@need_exe
def test_negative_control_one_step_solver_reads_red(portlib):
    """A start eight tiles off: worldToScreen of the start is far from the target, which a solver of one step would leave."""
    st, T, F = _state(portlib, 50)
    try:
        _aim(portlib, st, 1, (25, 25))
        _slope(st)
        sx, sy = _w2s(st, 25 << 10, 25 << 10)
        start = _w2s(st, (17 << 10) + 0x200, (17 << 10) + 0x200)
        assert math.hypot(start[0] - sx, start[1] - sy) > 1.0, "the control is blind: the start already projects there"
        r = _point(st, sx, sy, 17, 17, 0)
        assert math.hypot(r[6] / 16 - sx, r[7] / 16 - sy) <= 0.5
    finally:
        _BUF.pop(id(st), None)
        st.close()


@need_exe
@pytest.mark.parametrize("zoom,radius", [(1, 0x800), (2, 0x400), (4, 0x200), (8, 0x200)])
def test_snap_radius_and_green_rule(portlib, zoom, radius):
    st, T, F = _state(portlib, 50)
    try:
        o = objs(portlib)
        st.write(o["g_zoomScale"][0], struct.pack("<i", zoom))
        # hole 1's cup: tile (10, 9) inside its 4 x 4 green (layout: x 9..12, y 8..11); _set_cup writes g_holes' cup tile
        _set_cup(portlib, st, 1, 10, 9)
        _aim(portlib, st, 1, (10, 9))
        qx, qy = (10 << 10) + 0x200, (9 << 10) + 0x200
        for d, want in ((0, 1), (radius - 40, 1), (radius + 40, 0)):
            for ang in (0.3, 2.0, 4.1):
                wx, wy = int(qx + d * math.cos(ang)), int(qy + d * math.sin(ang))
                if T[(wx >> 10) * 50 + (wy >> 10)] != 1:
                    continue
                sx, sy = _w2s(st, wx, wy)
                px, py, snap, rx, ry, rad = _point(st, sx, sy, wx >> 10, wy >> 10, 1)[:6]
                assert rad == radius
                if math.hypot(rx - qx, ry - qy) > radius:
                    want_here = 0
                else:
                    want_here = 1
                assert snap == want_here, (d, ang, rx, ry)
                if snap:
                    assert (px, py) == (qx, qy)
        # a point within the radius but on a tile off the green: no snap (the cup moved to the green's edge tile (9, 8))
        _set_cup(portlib, st, 1, 9, 8)
        wx, wy = (9 << 10) - 60, (8 << 10) + 0x200      # tile (8, 8): rough
        assert T[8 * 50 + 8] != 1
        sx, sy = _w2s(st, wx, wy)
        px, py, snap = _point(st, sx, sy, 8, 8, 1)[:3]
        assert snap == 0
    finally:
        _BUF.pop(id(st), None)
        st.close()


# ---------------------------------------------------------------------------------------------------- the real shot
def _seg_ok():
    from port.test import segments as S
    return S.SEG_DIR.exists() and S.INSTALL.is_dir()


def _aiming_state(portlib, st, name="cov_a_champ/00"):
    from port.test import segments as S
    sc, k = name.split("/")
    seg = S.load_segment(S.SEG_DIR / sc / k)
    st.set_data_base(S.DATA_VA)
    st.files_root(S.INSTALL)
    st.load_image(seg.start)
    return seg


@need_exe
@pytest.mark.skipif(not _seg_ok(), reason="segment corpus or install not present")
def test_snap_shot_equals_cup_tile_shot_and_free_point_turns_it(portlib):
    o = objs(portlib)
    G = o["g_golfers"][0]
    with portlib.state() as st:
        _aiming_state(portlib, st)
        img = bytearray(st.image())
    g = struct.unpack_from("<i", img, o["g_playerGolfer"][0])[0]
    rec = G + (g << 8)
    hole = struct.unpack_from("<b", img, rec + 0x21)[0]
    cup = struct.unpack_from("<ii", img, o["g_holes"][0] + 0x208 * hole + 0x18)
    # the click (mainLoop ML+0xfd18): the aim tile becomes the target +0xcc / +0xd0 (here: the cup tile)
    struct.pack_into("<ii", img, rec + 0xc4, *cup)
    struct.pack_into("<ii", img, rec + 0xcc, *cup)

    def run(x, y):
        with portlib.state() as st:
            _aiming_state(portlib, st)
            st.load_image(bytes(img))
            if x == -1:
                st.call("prepareShot", [g, 1, 0xFFFFFFFF, 0, 0])
            else:
                st.call("prepareShot.free", [g, x & 0xFFFFFFFF, y & 0xFFFFFFFF])
            return st.image()
    orig = run(-1, 0)
    snap = run((cup[0] << 10) + 0x200, (cup[1] << 10) + 0x200)
    assert snap == orig, "the snapped point's shot differs from the cup-tile shot"
    off = run((cup[0] << 10) + 0x380, (cup[1] << 10) + 0x80)
    d0 = struct.unpack_from("<i", orig, rec + 0xe0)[0]
    d1 = struct.unpack_from("<i", off, rec + 0xe0)[0]
    assert d0 != d1, "an off-centre free point did not change the shot's direction"
    # the target tile fields stay the point's tile
    assert struct.unpack_from("<ii", off, rec + 0xcc) == cup


@need_exe
@pytest.mark.skipif(not _seg_ok(), reason="segment corpus or install not present")
def test_frame_run_switch(portlib):
    from port.test import integrator as I
    from port.test import segments as S
    o = objs(portlib)
    held = {}

    def after(st, r):
        b = (C.c_int32 * 8)()
        st.map(OUT_VA, b)
        st.call("aimFree.held", [OUT_VA])
        held["v"] = tuple(b[:6])
        img = st.image()
        g = struct.unpack_from("<i", img, o["g_playerGolfer"][0])[0]
        held["aim"] = struct.unpack_from("<ii", img, o["g_golfers"][0] + (g << 8) + 0xc4) if 0 <= g < 0x98 else None
        held["g"] = g

    any_held = 0
    try:
        for name in ("cov_a_champ/00", "cov_a_champ/01", "cov_a_play/01", "cov_a_play/02"):
            sc, k = name.split("/")
            seg = S.load_segment(S.SEG_DIR / sc / k)
            portlib.defect(SW, True)
            r1 = I.run_from(portlib, seg.name, seg, seg.trace, seg.records, None, after=after)
            assert r1.status == "PASS", (name, r1.status, r1.detail)
            assert held["v"][0] == -1, (name, "the original's lattice held a free point")
            portlib.defect(SW, False)
            r0 = I.run_from(portlib, seg.name, seg, seg.trace, seg.records, None, after=after)
            print(name, "switch 0:", r0.status, r0.frames_run, "held", held["v"], "player", held["g"], "aim tile", held["aim"])
            g, x, y = held["v"][:3]
            if g >= 0 and g == held["g"]:   # (a run whose player's turn ended keeps an unused point: keyed, never applied)
                any_held += 1
                assert (x >> 10, y >> 10) == held["aim"], (name, x, y, held["aim"])
    finally:
        portlib.defect(SW, True)
    assert any_held > 0, "no segment ended with a held free point"
