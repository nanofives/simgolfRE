"""P35E item 7: the play switch clickAmenityPanel.categoryPressKeepsList.

The amenities palette's five category buttons (pickAmenity434980 hotspots 3, 9, 4, 5, 6: benches, bridges, landmarks, flower beds,
scenic trees) set the amenity tool f_w_4c2848[3] to 1, 0x13, 4, 2, 0x10 whatever it holds. With the switch at 1 (the original) a
press on the button of the tool already held keeps that tool (drawAmenitiesPanel keeps drawing its item list); at 0 it clears the
tool and g_panelState[0] / [4] to -1 (the list closes, no item picked). Every other press gives the original's result."""
from __future__ import annotations

import struct

import pytest

from port.test import test_commands as TC

SWITCH = "clickAmenityPanel.categoryPressKeepsList"
# (hotspot centre in 800-space, the tool the button sets)
CATS = {"benches": ((0x1d6, 0x23f), 1), "bridges": ((0x213, 0x244), 0x13), "landmarks": ((0x24f, 0x23f), 4),
        "flowers": ((0x28c, 0x23f), 2), "trees": ((0x2ca, 0x23f), 0x10)}


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def _image(lib, tool, picked=3):
    img = bytearray(TC.base_image(lib))
    TC.put32(lib, img, "g_flags", 0)
    TC.put32(lib, img, "g_buildPanelSlot", 1)
    TC.put32(lib, img, "f_w_4c2848", tool, 12)
    TC.put32(lib, img, "g_panelState", picked, 0)
    TC.put32(lib, img, "g_panelState", picked, 16)
    return bytes(img)


def _words(lib, image):
    o = TC.objs(lib)
    tool = struct.unpack_from("<i", image, o["f_w_4c2848"][0] + 12)[0]
    ps = o["g_panelState"][0]
    return tool, struct.unpack_from("<i", image, ps)[0], struct.unpack_from("<i", image, ps + 16)[0]


def _both(lib, name, xy, img):
    lib.defect(SWITCH, True)
    a = TC.port_call(lib, name, list(xy), img)
    lib.defect(SWITCH, False)
    b = TC.port_call(lib, name, list(xy), img)
    return a, b


@TC.need_exe
@pytest.mark.parametrize("cat", list(CATS))
def test_original_second_press_keeps_the_list(portlib, cat):
    xy, tool = CATS[cat]
    assert portlib.defect(SWITCH, True) == 1
    a = TC.port_call(portlib, "clickAmenityPanel", list(xy), _image(portlib, tool))
    assert a[2] == 0 and _words(portlib, a[1])[0] == tool


@TC.need_exe
@pytest.mark.parametrize("cat", list(CATS))
@pytest.mark.parametrize("entry", ["clickAmenityPanel", "clickSlotPanel"])
def test_play_second_press_closes_the_list(portlib, cat, entry):
    """clickSlotPanel forwards to clickAmenityPanel while the palette is open (g_buildPanelSlot), as mainLoop calls it"""
    xy, tool = CATS[cat]
    assert portlib.defect(SWITCH, False) == 1
    a = TC.port_call(portlib, entry, list(xy), _image(portlib, tool))
    assert a[2] == 0 and _words(portlib, a[1]) == (-1, -1, -1)
    if entry == "clickAmenityPanel":
        assert a[0] == 0xFFFFFFFF


@TC.need_exe
@pytest.mark.parametrize("cat", list(CATS))
@pytest.mark.parametrize("held", [-1, 1, 0x13, 4, 2, 0x10, 3, 0])
def test_play_first_press_is_the_original(portlib, cat, held):
    """a press on a category whose tool is not held (another category, a left item's tool, none): the original's result"""
    xy, tool = CATS[cat]
    if held == tool:
        pytest.skip("the second press")
    a, b = _both(portlib, "clickAmenityPanel", xy, _image(portlib, held))
    assert (a[0], a[1], a[2]) == (b[0], b[1], b[2])
    assert _words(portlib, b[1])[0] == tool


@TC.need_exe
@pytest.mark.parametrize("held", [1, 0x13, 4, 2, 0x10])
@pytest.mark.parametrize("xy", [(0x12e, 0x233), (0x14d, 0x20e), (0x16b, 0x233), (0x10d, 500), (0x200, 0x1f4), (0x230, 0x1fa),
                                (0x1c0, 0x150)])
def test_play_other_presses_are_the_original(portlib, held, xy):
    """the left items, the close knob, row items and blank space with a category held: unchanged"""
    a, b = _both(portlib, "clickAmenityPanel", xy, _image(portlib, held))
    assert (a[0], a[1], a[2]) == (b[0], b[1], b[2]), xy
