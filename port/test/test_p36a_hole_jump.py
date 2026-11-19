"""P36A (Mariano's play-test 18 item 1: "when i double click on a hole on the RECORRIDO it should take me straight to the hole").

showRoutingMapScreen (port/src/sim/routing.cpp) reads view block word +0x94 (bridge.h kPortViewDoubleVa), which a live host writes
under the app option --hole-jump at the delivery of each left click (1 = the second click of a double click). With the word set, a
left click on the row of a listed hole (mode -1, the hole has a par) leaves the map with the camera halfway between the hole's tee
(+8 / +0xc) and green (+0x18 / +0x1c), as the original's hole keys put it, clamped to the camera's range (0..0x31 here: the
cameraBoundByZoom switch at 1), and the click's button word cleared so the frame body does not take it as a course click. With the
word clear (no host writes it without the option) the same input is the original's: a selection, and the screen goes on.
The word is set by the test-only entry bridge.viewDouble.
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
    TC.put32(lib, img, "gCameraTileX", 7)
    TC.put32(lib, img, "gCameraTileY", 9)
    for h in range(1, 19):
        TC.put(lib, img, "g_holes", bytes([0]), HOLE * h)
    for h, (par, tee, green) in (holes or {}).items():
        TC.put(lib, img, "g_holes", bytes([par]), HOLE * h)
        TC.put(lib, img, "g_holes", struct.pack("<ii", *tee), HOLE * h + 8)
        TC.put(lib, img, "g_holes", struct.pack("<ii", *green), HOLE * h + 0x18)
        TC.put(lib, img, "g_holes", struct.pack("<i", 0), HOLE * h + 0x20)   # no rounds counted: the list's time row divides by it
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
    return get("gCameraTileX"), get("gCameraTileY"), get("g_rootInput", 16), used


def _row(h):
    """a point on hole h's row of the list (left column 1..9, right column 10..18)"""
    return (0x60 if h <= 9 else 0x250), 0x68 + 17 * ((h - 1) % 9) + 2


HOLES = {3: (4, (10, 20), (15, 27)), 12: (5, (40, 3), (31, 8)), 6: (3, (70, -6), (80, 2))}


def test_double_click_goes_to_the_hole(portlib):
    img = _image(portlib, holes=HOLES)
    x, y = _row(3)
    assert _run(portlib, img, CE.click(x, y), 1) == (12, 23, 0, 2)   # (10 + 15) / 2, (20 + 27) / 2; the map left at the click (its onUnpoint is the next pump's)
    x, y = _row(12)
    assert _run(portlib, img, CE.click(x, y), 1) == (35, 5, 0, 2)    # the right column
    x, y = _row(6)
    assert _run(portlib, img, CE.click(x, y), 1) == (0x31, 0, 0, 2)  # (75, -2) clamped to the camera's range


def test_without_the_word_the_click_selects_as_the_original(portlib):
    img = _image(portlib, holes=HOLES)
    x, y = _row(3)
    cx, cy, _, used = _run(portlib, img, CE.click(x, y) + END, 0)
    assert (cx, cy, used) == (7, 9, 5)


def test_no_jump_off_a_listed_hole_or_outside_the_list_mode(portlib):
    x, y = _row(4)   # hole 4 has no par: not listed
    assert _run(portlib, _image(portlib, holes=HOLES), CE.click(x, y) + END, 1)[:2] == (7, 9)
    assert _run(portlib, _image(portlib, holes=HOLES), CE.click(0x150, _row(3)[1]) + END, 1)[:2] == (7, 9)  # between the columns
    x, y = _row(3)
    assert _run(portlib, _image(portlib, mode=0, holes=HOLES), CE.click(x, y) + END, 1)[:2] == (7, 9)  # the walker mode
