"""P38H (Mariano 2026-11-19: "cuando picky juegue y le guste mi terreno y me quiera ofrecer, quiero que quede disponible para comprar
2 terrenos y poder comprarlos después"; log/roadmap/P38/P38H_notes.md).

Play switch mainLoop.landOfferOnce (port/src/sim/landoffer.h; 1 = the original): at 0 I.M.Picky's approval tops the current course's
land credits (byte +0x21 of its g_courseTable record, a byte only the whole-table clear, save and load touch) up to 2, and the routing
map (showRoutingMapScreen, port/src/sim/routing.cpp) shows a Buy land button in the corner left of its title while credits remain.
A left click on it leaves the map with bit 1 of g_eventFlags567a1c set (mainLoop's frame then opens the buy-land screen without the
popup and takes a credit per purchase: tick_frame.inc P38H_Buy, judged in the app, notes section 6) and the click's button word
cleared. With the switch at 1, or with no credits, the same click is the original's: a hole row selection, and the screen goes on
until the closing click. Switch showBuyLandScreen.lotLabelLeftover is drawing only (judged by the app screenshots).
Negative control: showBuyLandScreen mode 10 reads the credits from the next course's record: the button then shows for the other
course's credits and hides for this course's (test_negative_control_reads_red).
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
REC, CREDITS = 0x2E, 0x21          # CourseRecord stride; the credits byte
BUTTON = (0x3C, 0x26)              # inside the Buy land button's box (0x10..0xbe, 6..0x46)
END = CE.click(0x100, 0x108)       # the strip under the map: ends the screen (the original's click branch)


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def _image(lib, course=3, credits=None, other=None):
    img = bytearray(TR._routing_image(lib, random.Random("P38H"), (0, 0), bad_golfer=False))
    TC.put32(lib, img, "g_ui_4d20dc", -1)
    TC.put32(lib, img, "g_map822b80", 0)
    for nm in ("g_rootInput", "gDirtyFlagA", "gDirtyFlagB", "g_redrawPending", "g_flag_83afcc"):
        TC.put(lib, img, nm, bytes(TC.objs(lib)[nm][1]))
    TC.put32(lib, img, "g_eventFlags567a1c", 0)
    TC.put32(lib, img, "g_courseIndex", course)
    for c in range(17):
        TC.put(lib, img, "g_courseTable", bytes([0]), REC * c + CREDITS)
    if credits is not None:
        TC.put(lib, img, "g_courseTable", bytes([credits]), REC * course + CREDITS)
    if other is not None:
        TC.put(lib, img, "g_courseTable", bytes([other]), REC * (course + 1) + CREDITS)
    return bytes(img)


def _run(lib, img, inputs, cursor=BUTTON):
    import ctypes as C
    with lib.state() as st:
        st.set_data_base(TC.DATA_VA)
        st.load_image(img)
        words = [w for c in inputs for w in c.args()]
        buf = (C.c_uint32 * len(words))(*words)
        st.map(TC.INPUT_VA, buf, readonly=True)
        st.call("commands.setInput", [TC.INPUT_VA, len(inputs)])
        st.call("commands.setCursor", list(cursor))
        st.call(TR.NAME, [])
        used = st.call("commands.inputConsumed", []).eax
        st.call("commands.clearInput", [])
        out = st.image()
    o = TC.objs(lib)

    def get(nm, at=0, fmt="<i"):
        return struct.unpack_from(fmt, out, o[nm][0] + at)[0]
    return {"flags": get("g_eventFlags567a1c"), "button": get("g_rootInput", 16), "used": used,
            "credits": get("g_courseTable", REC * 3 + CREDITS, "<B")}


def test_switches_registered_default_original(portlib):
    d = portlib.defects()
    for name in ("mainLoop.landOfferOnce", "showBuyLandScreen.lotLabelLeftover"):
        assert name in d, name
        assert d[name]["on"] == 1, name


def test_button_leaves_the_map_and_asks_for_the_screen(portlib):
    portlib.defect("mainLoop.landOfferOnce", False)
    r = _run(portlib, _image(portlib, credits=2), CE.click(*BUTTON))
    assert r["flags"] == 1 << 1, r        # the buy-now request, nothing else
    assert r["button"] == 0, r            # the click is the map's
    assert r["used"] == 2, r              # the press and its release only: the closing click is not needed
    assert r["credits"] == 2, r           # the map takes no credit (the purchase does)
    r = _run(portlib, _image(portlib, credits=1), CE.click(*BUTTON))
    assert r["flags"] == 1 << 1, r


def test_switch_at_1_is_the_original(portlib):
    # the same click with credits in the record: a hole row selection, the map goes on until the closing click
    r = _run(portlib, _image(portlib, credits=2), CE.click(*BUTTON) + END)
    assert r["flags"] == 0, r
    assert r["used"] == 5, r   # as the P36A test: the click, the redraw pass, the closing click
    assert r["credits"] == 2, r


def test_no_credits_no_button(portlib):
    portlib.defect("mainLoop.landOfferOnce", False)
    for img in (_image(portlib, credits=0), _image(portlib, credits=None, other=2)):   # none, or another course's credits
        r = _run(portlib, img, CE.click(*BUTTON) + END)
        assert r["flags"] == 0, r
        assert r["used"] == 5, r   # as the P36A test: the click, the redraw pass, the closing click


def test_outside_the_box_is_the_original(portlib):
    portlib.defect("mainLoop.landOfferOnce", False)
    for pt in ((0xC0, 0x26), (0x3C, 0x48)):   # just right of the box, just under it
        r = _run(portlib, _image(portlib, credits=2), CE.click(*pt) + END, cursor=pt)
        assert r["flags"] == 0, (pt, r)


def test_course_index_out_of_the_table(portlib):
    portlib.defect("mainLoop.landOfferOnce", False)
    img = bytearray(_image(portlib, credits=2))
    TC.put32(portlib, img, "g_courseIndex", 17)   # no record: the credits are off
    r = _run(portlib, bytes(img), CE.click(*BUTTON) + END)
    assert r["flags"] == 0, r


def test_negative_control_reads_red(portlib):
    portlib.defect("mainLoop.landOfferOnce", False)
    portlib.negctl("showBuyLandScreen", 10)
    try:
        r = _run(portlib, _image(portlib, credits=None, other=2), CE.click(*BUTTON) + END)
        assert r["flags"] == 1 << 1, r      # RED of test_no_credits_no_button: another course's credits show the button
        r = _run(portlib, _image(portlib, credits=2), CE.click(*BUTTON) + END)
        assert r["flags"] == 0, r           # RED of test_button_leaves_the_map_and_asks_for_the_screen
    finally:
        portlib.negctl("showBuyLandScreen", 0)
