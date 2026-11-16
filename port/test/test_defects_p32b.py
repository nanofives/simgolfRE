"""P32B item 4: the play switch clickGolferPanel.namePlateCentresCamera.

In a match (the player golfer's record word +0xcc != -1) drawMatchScoreboard writes the player golfer's name on the upper plate of
the scorecard and its partner's (record word +0xa2) on the lower one. With the switch at 1 (the original) a press there changes
nothing but what clickGolferPanel's hotspots do; at 0 it puts the camera tile pair on that golfer's position >> 10 (then the camera
bound), as clickEmployeeView's golfer slot does."""
from __future__ import annotations

import random
import struct

import pytest

from port.test import test_commands as TC

SWITCH = "clickGolferPanel.namePlateCentresCamera"
P, PARTNER = 3, 9


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def _image(lib, match=True, seed=1):
    rng = random.Random(seed)
    img = bytearray(TC.base_image(lib))
    TC.rand_obj(lib, img, "g_golfers", rng)
    TC.put32(lib, img, "g_panelListMode", 3)
    TC.put32(lib, img, "g_ui_561254", 0)
    TC.put32(lib, img, "g_playerGolfer", P)
    TC.put32(lib, img, "g_flags", 0)
    TC.put32(lib, img, "g_holesPlusOne", 10)
    TC.put32(lib, img, "gCameraTileX", 7)
    TC.put32(lib, img, "gCameraTileY", 8)
    TC.put32(lib, img, "g_golfers", 0 if match else 0xFFFFFFFF, (P << 8) + 0xcc)
    TC.put(lib, img, "g_golfers", struct.pack("<h", PARTNER), (P << 8) + 0xa2)
    for g, (x, y) in ((P, (20, 30)), (PARTNER, (35, 12))):
        TC.put32(lib, img, "g_golfers", (x << 10) + 0x155, g << 8)
        TC.put32(lib, img, "g_golfers", (y << 10) + 0x2aa, (g << 8) + 4)
    return bytes(img)


def _camera(lib, image):
    o = TC.objs(lib)
    x = struct.unpack_from("<i", image, o["gCameraTileX"][0])[0]
    y = struct.unpack_from("<i", image, o["gCameraTileY"][0])[0]
    return x, y


PLATE1, PLATE2, OFF = (0x170, 0x22e), (0x170, 0x244), (0x170, 0x210)


@TC.need_exe
@pytest.mark.parametrize("xy", [PLATE1, PLATE2])
def test_original_plate_press_leaves_the_camera(portlib, xy):
    assert portlib.defect(SWITCH, True) == 1
    img = _image(portlib)
    a = TC.port_call(portlib, "clickGolferPanel", list(xy), img)
    assert a[2] == 0 and _camera(portlib, a[1]) == (7, 8)


@TC.need_exe
@pytest.mark.parametrize("xy,want", [(PLATE1, (20, 30)), (PLATE2, (35, 12)), ((0x12e, 0x244), (35, 12))])
def test_play_plate_press_centres_on_its_golfer(portlib, xy, want):
    """the knob too (hotspot 3, which acts only with no player golfer); the camera bound keeps an on-grid tile as it is"""
    assert portlib.defect(SWITCH, False) == 1
    a = TC.port_call(portlib, "clickGolferPanel", list(xy), _image(portlib))
    assert a[2] == 0 and _camera(portlib, a[1]) == want


@TC.need_exe
def test_play_only_in_a_match_and_on_a_plate(portlib):
    """out of a match, or a press off the plates: the same image as the original"""
    for img, xy in ((_image(portlib, match=False), PLATE1), (_image(portlib), OFF), (_image(portlib), (0x1c0, 0x22e))):
        portlib.defect(SWITCH, True)   # sg_defect_set returns the switch's previous value
        a = TC.port_call(portlib, "clickGolferPanel", list(xy), img)
        portlib.defect(SWITCH, False)
        b = TC.port_call(portlib, "clickGolferPanel", list(xy), img)
        assert (a[0], a[1], a[2]) == (b[0], b[1], b[2]), xy
