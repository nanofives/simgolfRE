"""P33C (play-test 15 item 3): tileToScreen.spriteReach.

The tile pass (mainLoop's per-tile loop) queues a tile's scenery, trees and placed object only when tileToScreen accepts the tile:
its centre within x -0x40 .. 800 + 0x40 and y -0x2a .. 600 + 0x2a. At the port's zoom 8 a tree's art reaches up to 168 units
above its centre and a placed object's up to 752 sideways, so they vanished near the screen's edges. At 0 the window is the tile
sprites' reach at the zoom (bridge.h tile_sprite_reach: x 18, above the top 6, below the bottom 22 units per zoom step, never
less than the original's), and the projection keeps the point's height off screen. Flat zeroed terrain here (type flag 8: no
height): the point is sx = 64 (a + b) + 464, sy = 40 (b - a) + 300 at zoom 8 for tile (25 + a, 25 + b), camera on tile (25, 25).
"""
from __future__ import annotations

import struct

import pytest

pytestmark = pytest.mark.port
SWITCH = "tileToScreen.spriteReach"
OUT_X, OUT_Y = 0x40000010, 0x40001020


def off(lib, name):
    return next(o for n, o, s in lib.objects() if n == name)


def _project(lib, tx, ty, zoom=8):
    with lib.state() as st:
        for name, v in (("g_zoomScale", zoom), ("gCameraTileX", 25), ("gCameraTileY", 25), ("g_screenWidth", 800),
                        ("g_screenHeight", 600), ("g_cameraRot", 0)):
            st.write(off(lib, name), struct.pack("<i", v))
        # tile type 0 (every tile of the zeroed grid) with flag 8: worldToScreen adds no height (its record's flags at +0x2c)
        st.write(off(lib, "g_tileTypeAttrs") + 0x2c, struct.pack("<I", 8))
        bx = st.map(OUT_X, bytearray(4))
        by = st.map(OUT_Y, bytearray(4))
        r = st.call("tileToScreen", [tx, ty, OUT_X, OUT_Y]).eax
        return r, struct.unpack("<i", bytes(bx))[0], struct.unpack("<i", bytes(by))[0]


@pytest.mark.parametrize("on", [1, 0])
def test_window_follows_the_switch(portlib, on):
    try:
        portlib.defect(SWITCH, on)
        inside = _project(portlib, 26, 26)    # (592, 300): inside the screen
        right = _project(portlib, 28, 29)     # (912, 340): 112 past the right edge (original margin 64, zoom 8 reach 144)
        below = _project(portlib, 21, 30)     # (528, 660): 60 below the bottom edge (original 42, zoom 8 reach 176)
        far = _project(portlib, 30, 31)       # (1168, 340): past both
        right_z2 = _project(portlib, 30, 31, zoom=2)  # zoom 2: 16 (a + b) + 416 = 592, inside at both
        edge_z2 = _project(portlib, 49, 49, zoom=2)   # zoom 2: (1184, 300): past the original 64 and zoom 2's 36 (max 64)
    finally:
        portlib.defect(SWITCH, 1)
    assert inside == (1, 592, 300)
    assert far[0] == 0
    assert right_z2[0] == 1 and edge_z2[0] == 0
    assert right[0] == (0 if on else 1) and below[0] == (0 if on else 1)
    if not on:
        assert right[1:] == (912, 340) and below[1:] == (528, 660)


def test_switch_is_registered_original_by_default(portlib):
    assert SWITCH in portlib.defects()
    assert _project(portlib, 28, 29)[0] == 0
