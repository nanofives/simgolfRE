"""P36A (Mariano's play-test 18 item 1: "when i double click on a hole on the RECORRIDO it should take me straight to the hole").

showRoutingMapScreen (port/src/sim/routing.cpp) reads view block word +0x94 (bridge.h kPortViewDoubleVa), which a live host writes
under the app option --hole-jump at the delivery of each left click (1 = the second click of a double click). With the word set, a
left click on the row of a listed hole (mode -1, the hole has a par) or (fix 2) on a hole number of the small map (every mode but
0) leaves the map with the camera halfway between the hole's tee (+8 / +0xc) and green (+0x18 / +0x1c), as the original's hole
keys put it, clamped to the camera's range (0..0x31 here: the cameraBoundByZoom switch at 1), and the click's button word cleared
so the frame body does not take it as a course click. Fix 2: then the key's own hole stats request: g_msgState53df54 = 0, and -hole
when tileToScreen of the green returns nonzero (at the moved camera). With the word clear (no host writes it without the option)
the same input is the original's: a selection, a redraw, and the screen goes on. The word is set by the test-only entry
bridge.viewDouble.
"""
from __future__ import annotations

import random
import struct

import pytest

from port.test import cmdemu as CE
from port.test import lib as L
from port.test import test_commands as TC
from port.test import test_routing as TR

pytestmark = [pytest.mark.port,
              pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")]
HOLE = 0x208
END = CE.click(0x100, 0x108)   # the strip under the map: ends the screen (the original's click branch)


def _image(lib, mode=-1, holes=None):
    img = bytearray(TR._routing_image(lib, random.Random("P36A"), (0, 0), bad_golfer=False))
    TC.put32(lib, img, "g_ui_4d20dc", mode)
    TC.put32(lib, img, "g_map822b80", 0)
    for nm in ("g_rootInput", "gDirtyFlagA", "gDirtyFlagB", "g_redrawPending", "g_flag_83afcc"):
        TC.put(lib, img, nm, bytes(TC.objs(lib)[nm][1]))
    for nm, v in (("gCameraTileX", 7), ("gCameraTileY", 9), ("g_zoomScale", 4), ("g_cameraRot", 0), ("g_screenWidth", 800),
                  ("g_screenHeight", 600), ("g_msgState53df54", 5)):
        TC.put32(lib, img, nm, v)
    for h in range(1, 19):
        TC.put(lib, img, "g_holes", bytes([0]), HOLE * h)
    for h, (par, tee, green) in (holes or {}).items():
        TC.put(lib, img, "g_holes", bytes([par]), HOLE * h)
        TC.put(lib, img, "g_holes", struct.pack("<ii", *tee), HOLE * h + 8)
        TC.put(lib, img, "g_holes", struct.pack("<ii", *green), HOLE * h + 0x18)
        TC.put(lib, img, "g_holes", struct.pack("<i", 0), HOLE * h + 0x20)   # no rounds counted: the list's time row divides by it
        TC.put(lib, img, "g_holePoints", struct.pack("<6i", *([-1] * 6)), 0x18 * h)   # no waypoint: the number sits mid-line
    return bytes(img)


def _run(lib, img, inputs, word):
    import ctypes as C
    with lib.state() as st:
        st.set_data_base(TC.DATA_VA)
        st.load_image(img)
        assert st.call("bridge.viewDouble", [word]).eax == (1 if word else 0)
        words = [w for c in inputs for w in c.args()]
        buf = (C.c_uint32 * len(words))(*words)
        st.map(TC.INPUT_VA, buf, readonly=True)
        st.call("commands.setInput", [TC.INPUT_VA, len(inputs)])
        st.call("commands.setCursor", [0, 0])
        st.call(TR.NAME, [])
        used = st.call("commands.inputConsumed", []).eax
        st.call("commands.clearInput", [])
        out = st.image()
    o = TC.objs(lib)

    def get(nm, at=0):
        return struct.unpack_from("<i", out, o[nm][0] + at)[0]
    return get("gCameraTileX"), get("gCameraTileY"), get("g_rootInput", 16), used, get("g_msgState53df54")


def _row(h):
    """a point on hole h's row of the list (left column 1..9, right column 10..18)"""
    return (0x60 if h <= 9 else 0x250), 0x68 + 17 * ((h - 1) % 9) + 2


def _label(tee, green):
    """the small map's number of a hole without a waypoint: the middle of the projected tee and green (mapToScreen: x = 6 (a + b)
    + 0x6a, y = 3 (b - a) + 0x1b8 + 1 with g_map822b80[0] = 0), y - 4"""
    def ms(a, b):
        return 6 * (a + b) + 0x6A, 3 * (b - a) + 0x1B8 + 1
    (tx, ty), (gx, gy) = ms(*tee), ms(*green)
    return int((gx + tx) / 2), int((gy + ty) / 2) - 4


HOLES = {3: (4, (10, 20), (15, 27)), 12: (5, (40, 3), (31, 8)), 6: (3, (70, -6), (48, 2)), 7: (4, (-60, 0), (40, 40))}


def test_double_click_goes_to_the_hole(portlib):
    img = _image(portlib, holes=HOLES)
    x, y = _row(3)
    # (10 + 15) / 2, (20 + 27) / 2; the map left at the click (its onUnpoint is the next pump's); the green on screen: stats of 3
    assert _run(portlib, img, CE.click(x, y), 1) == (12, 23, 0, 2, -3)
    x, y = _row(12)
    assert _run(portlib, img, CE.click(x, y), 1) == (35, 5, 0, 2, -12)   # the right column
    x, y = _row(6)
    assert _run(portlib, img, CE.click(x, y), 1)[:4] == (0x31, 0, 0, 2)  # (59, -2) clamped to the camera's range
    x, y = _row(7)
    assert _run(portlib, img, CE.click(x, y), 1) == (0, 20, 0, 2, 0)     # (-10, 20) clamped; its green is off screen: no stats
    # the screen cache still says "off screen" for hole 3's green (the picture of the old camera): emptied, judged at the new camera
    stale = bytearray(img)
    TC.put(portlib, stale, "g_tileScreenX", struct.pack("<h", -99), 2 * (15 * 50 + 27))
    x, y = _row(3)
    assert _run(portlib, bytes(stale), CE.click(x, y), 1) == (12, 23, 0, 2, -3)


def test_double_click_on_the_small_map(portlib):
    tee, green = HOLES[3][1:]
    lx, ly = _label(tee, green)
    for mode in (-1, 1, 2):
        img = _image(portlib, mode=mode, holes=HOLES)
        assert _run(portlib, img, CE.click(lx, ly + 4), 1) == (12, 23, 0, 2, -3), mode
        assert _run(portlib, img, CE.click(lx + 9, ly - 4), 1)[:2] == (12, 23), mode    # the box's corner
    img = _image(portlib, mode=1, holes=HOLES)
    assert _run(portlib, img, CE.click(lx + 10, ly + 4) + END, 1)[:2] == (7, 9)    # past the box: the button row's redraw
    assert _run(portlib, img, CE.click(lx, ly + 15) + END, 1)[:2] == (7, 9)
    img = _image(portlib, mode=0, holes=HOLES)   # the walker mode draws no numbers
    assert _run(portlib, img, CE.click(lx, ly + 4) + END, 1)[:2] == (7, 9)


def test_without_the_word_the_click_selects_as_the_original(portlib):
    img = _image(portlib, holes=HOLES)
    x, y = _row(3)
    assert _run(portlib, img, CE.click(x, y) + END, 0) == (7, 9, 1, 5, 5)   # the closing click keeps its button word, as in the original
    lx, ly = _label(*HOLES[3][1:])
    assert _run(portlib, _image(portlib, mode=1, holes=HOLES), CE.click(lx, ly + 4) + END, 0)[:2] == (7, 9)


def test_no_jump_off_a_listed_hole_or_outside_the_list_mode(portlib):
    x, y = _row(4)   # hole 4 has no par: not listed
    assert _run(portlib, _image(portlib, holes=HOLES), CE.click(x, y) + END, 1)[:2] == (7, 9)
    assert _run(portlib, _image(portlib, holes=HOLES), CE.click(0x150, _row(3)[1]) + END, 1)[:2] == (7, 9)  # between the columns
    x, y = _row(3)
    assert _run(portlib, _image(portlib, mode=0, holes=HOLES), CE.click(x, y) + END, 1)[:2] == (7, 9)  # the walker mode
