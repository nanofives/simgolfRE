"""P27A fix 5: the play switch hitGolferPanel.shotButtonsAsDrawn and clickGolferPanel's forwarding.

clickGolferPanel hands every press to handleEmployeeAction while the staff detail flag g_ui_561254 is set, with no panel
mode test, although drawGolferActionPanel draws the golfer panel in mode 3 whatever that flag. With the switch at 1 (the
original) a mode-3 press is forwarded; at 0 it answers the golfer panel drawn. Mode 4 forwards either way."""
from __future__ import annotations

import pytest

from port.test import test_commands as TC

SWITCH = "hitGolferPanel.shotButtonsAsDrawn"


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def _cases(lib, mode, flag, n=40):
    for img, args, inputs in TC.panel_cases(lib, "clickGolferPanel", n, f"p27a5/{mode}/{flag}"):
        img = bytearray(img)
        TC.put32(lib, img, "g_panelListMode", mode)
        TC.put32(lib, img, "g_ui_561254", flag)
        yield bytes(img), args, inputs


def _masked(lib, image: bytes) -> bytes:
    img = bytearray(image)
    TC.put32(lib, img, "g_ui_561254", 0)
    return bytes(img)


@TC.need_exe
@pytest.mark.parametrize("mode", [3, 4])
def test_original_forwards_to_the_staff_panel(portlib, mode):
    """switch at 1: with g_ui_561254 set a press does exactly what handleEmployeeAction does, in mode 3 and in mode 4"""
    assert portlib.defect(SWITCH, True) == 1
    for img, args, inputs in _cases(portlib, mode, 1):
        a = TC.port_call(portlib, "clickGolferPanel", args, img, inputs)
        b = TC.port_call(portlib, "handleEmployeeAction", args, img, inputs)
        assert (a[0], a[1], a[2], a[3]) == (b[0], b[1], b[2], b[3]), args


@TC.need_exe
def test_play_mode3_press_answers_the_golfer_panel(portlib):
    """switch at 0: a mode-3 press with g_ui_561254 set does what the golfer panel does with the flag clear (the flag
    word itself aside); at least one case differs from the forwarded press"""
    assert portlib.defect(SWITCH, False) == 1
    differ = 0
    for img, args, inputs in _cases(portlib, 3, 1):
        a = TC.port_call(portlib, "clickGolferPanel", args, img, inputs)
        clear = bytearray(img)
        TC.put32(portlib, clear, "g_ui_561254", 0)
        b = TC.port_call(portlib, "clickGolferPanel", args, bytes(clear), inputs)
        assert (a[0], _masked(portlib, a[1]), a[2], a[3]) == (b[0], _masked(portlib, b[1]), b[2], b[3]), args
        f = TC.port_call(portlib, "handleEmployeeAction", args, img, inputs)
        differ += (a[0], a[1]) != (f[0], f[1])
    assert differ > 0


@TC.need_exe
def test_play_mode4_still_forwards(portlib):
    """switch at 0: mode 4 with g_ui_561254 set still forwards (mode 4 draws the staff detail panel then)"""
    assert portlib.defect(SWITCH, False) == 1
    for img, args, inputs in _cases(portlib, 4, 1):
        a = TC.port_call(portlib, "clickGolferPanel", args, img, inputs)
        b = TC.port_call(portlib, "handleEmployeeAction", args, img, inputs)
        assert (a[0], a[1], a[2], a[3]) == (b[0], b[1], b[2], b[3]), args
