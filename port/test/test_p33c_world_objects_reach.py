"""P33C (play-test 15 item 4): drawWorldObjects.spriteReach.

drawWorldObjects queues a ground animal or a flyer only while its ground point passes worldToScreen with margin 0. A blimp (sky
actor kind 2) draws its shadow at that point and its body up to (0x50 - 0x20) x zoom / 4 above it, with art reaching 181 units
(zoom 4) above the hotspot, so with its ground point just below the screen's bottom edge the original draws nothing while the body
is on screen. At 0 the point is projected again with a margin and drawn when the art can reach the view (bridge.h
world_object_reach); only the drawing changes: the RNG and the animals' records end the same at both values.
Flat zeroed terrain (type flag 8: no height), zoom 8, camera on tile (25, 25): tile (25 + a, 25 + b) is at
sx = 64 (a + b) + 464, sy = 40 (b - a) + 300.
"""
from __future__ import annotations

import struct

import pytest

pytestmark = pytest.mark.port
SWITCH = "drawWorldObjects.spriteReach"


def off(lib, name):
    return next(o for n, o, s in lib.objects() if n == name)


def _run(lib, actor_tile, animal_tile):
    """drawWorldObjects once; the queue count, the RNG words, the habitat records"""
    with lib.state() as st:
        w = lambda name, fmt, v, plus=0: st.write(off(lib, name) + plus, struct.pack("<" + fmt, v))  # noqa: E731
        for name, v in (("g_zoomScale", 8), ("gCameraTileX", 25), ("gCameraTileY", 25), ("g_screenWidth", 800),
                        ("g_screenHeight", 600), ("g_cameraRot", 0), ("g_spriteQueueHead", -1)):
            w(name, "i", v)
        w("g_tileTypeAttrs", "I", 8, 0x2c)
        cols = next(sz for n, o, sz in lib.objects() if n == "g_sheetCols")
        st.write(off(lib, "g_sheetCols"), struct.pack("<%di" % (cols // 4), *([1] * (cols // 4))))  # one frame per sheet
        st.write(off(lib, "g_flags"), bytes([4]))         # bit 2: the flyers do not move
        st.write(off(lib, "g_preferences"), bytes([0x20]))  # animals on
        hab = off(lib, "g_habitatSlots")
        for i in range(0x80):                              # every animal off but record 0
            st.write(hab + 0x14 * i + 0x12, bytes([0xff]))
        ax, ay = animal_tile
        st.write(hab, struct.pack("<ii", (ax << 10) + 0x200, (ay << 10) + 0x200))
        st.write(hab + 0x12, bytes([0]))
        sky = off(lib, "g_skyActors")
        for j in range(4):
            st.write(sky + 0x10 * j, struct.pack("<i", -1))
            st.write(sky + 0x10 * j + 0xd, bytes([3]))   # kind 3 (inactive, X = -1; its spawn roll is the RNG's only use)
        tx, ty = actor_tile
        st.write(sky, struct.pack("<iii", (tx << 10) + 0x200, (ty << 10) + 0x200, 0x50))
        st.write(sky + 0xd, bytes([2]))                   # actor 0: the blimp
        st.call("drawWorldObjects", [])
        n = struct.unpack("<i", st.read(off(lib, "g_spriteQueueCount"), 4))[0]
        return n, st.read(off(lib, "g_rng"), 16), st.read(hab, 0x14 * 0x80)


@pytest.mark.parametrize("on", [1, 0])
def test_points_below_the_edge(portlib, on):
    try:
        portlib.defect(SWITCH, on)
        inside = _run(portlib, (25, 25), (25, 25))    # both on screen: blimp 2 sprites, animal 1
        below = _run(portlib, (20, 30), (21, 29))     # blimp point (464, 700): 100 below; animal (464, 620): 20 below
        far = _run(portlib, (10, 40), (5, 45))        # (464, 1500) and (464, 1900): beyond both reaches
    finally:
        portlib.defect(SWITCH, 1)
    assert inside[0] == 3
    assert far[0] == 0
    assert below[0] == (0 if on else 3)


def test_only_the_drawing_changes(portlib):
    out = {}
    try:
        for on in (1, 0):
            portlib.defect(SWITCH, on)
            out[on] = _run(portlib, (20, 30), (21, 29))
    finally:
        portlib.defect(SWITCH, 1)
    assert out[1][1:] == out[0][1:]


def test_switch_is_registered_original_by_default(portlib):
    assert SWITCH in portlib.defects()
    assert _run(portlib, (20, 30), (21, 29))[0] == 0
