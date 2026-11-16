"""P32E (Mariano's play-test 14 item 11, log/roadmap/P32/P32E_notes.md): the golfer pair select screen's play switches.

showGolferPairSelect.anyClickLeaves (fix 2): 1 = the original, any press outside the slots leaves the screen; 0 = only a press on
the check button leaves (with one golfer chosen it plays the refusal sound and stays), Esc closes without pairing (the
original pairs two chosen golfers on Esc), any other key leaves as in the original.
showGolferPairSelect.noButtonHover (fix 3): 1 = the original, the check and book buttons never change; 0 = under the cursor the
check is drawn lit (the PairButtons cut the original loads and never draws: cell 4 of g_infoSprites821120) and the book is
lightened with its tip (the drop showGolferPairSelect.bookHover).

The cases are test_dialogs.py's pair select images (three golfers waiting at the first tee, the hit box word 0x149 = the slot
frame's cut width) run through the same scripted input (dialogs.setScript). A script that ends while the screen still waits
gives SG_E_NO_RESOURCE (the screen stayed open). Run as one file per process.
"""
from __future__ import annotations

import ctypes as C
import random

import pytest

from port.test import lib as L
from port.test import test_dialogs as TD

pytestmark = pytest.mark.port

LEAVES = "showGolferPairSelect.anyClickLeaves"
HOVER = "showGolferPairSelect.noButtonHover"
SLOT0, SLOT1 = (0x60, 0x60), (0x14F + 0x60, 0x60)
STRAY = (0x2F0, 0x100)       # right of the slot box (6 .. 0x149 + 0x14f), off the check button
CHECK = (0x2DA, 0x21B)       # the check button's centre (0x2b5 + 0x25, 0x1f6 + 0x25)
BOOK = (737, 102)            # the book button's centre (PairBase ring x 692..782, y 57..147)
LOG_VA, LOG_CAP = 0x33000000, 0x40000


@pytest.fixture(scope="module")
def plib():
    lib = L.load()
    if not lib.has("showGolferPairSelect") or LEAVES not in lib.defects():
        pytest.skip("the library has no P32E pair select switches")
    yield lib
    lib.defect(LEAVES, True)
    lib.defect(HOVER, True)


def _image(lib, seed=0x32E):
    rng = random.Random(seed)
    img = TD._dialog_image(lib, rng, -1)
    goff = TD.objs(lib)["g_golfers"][0]
    for g in range(0x98):
        img[goff + 0x100 * g + 0x21] = 0xFF if g in (10, 11, 12) else 0
        img[goff + 0x100 * g + 0xB8:goff + 0x100 * g + 0xBA] = b"\0\0"
    TD.put32(lib, img, "g_lastAllocatedGolfer", 20)
    TD.put32(lib, img, "g_infoSprites821120", 0x149, 0x44)
    TD.put32(lib, img, "g_rootInput", 0, 0x10)
    return bytes(img)


def _masked(lib, img):
    """the image without g_rootInput (the last press's point: the two exits press at different places)"""
    off, n = TD.objs(lib)["g_rootInput"]
    return img[:off] + bytes(n) + img[off + n:]


def _script(clicks, end_key=False):
    """each click: one idle pass on the point (the new hover redraws), then the press there"""
    from port.test import cmdemu as CE
    inputs, cursors = [], []
    for pt in clicks:
        cursors += [pt, pt]
        inputs += [CE.Raw(8, 0, 0)] + CE.click(*pt)
    if end_key:
        cursors += [cursors[-1] if cursors else (0, 0)]
        inputs += CE.key(0x1B, 0x1B)
    return inputs, cursors


def _run(lib, clicks, leaves, hover=True, end_key=False):
    lib.defect(LEAVES, leaves)
    lib.defect(HOVER, hover)
    inputs, cursors = _script(clicks, end_key)
    eax, image, code, used, reads, _calls = TD.port_run(lib, "showGolferPairSelect", _image(lib), [], inputs, cursors, (5, 0))
    return eax, image, code, used, len(inputs)


def test_stray_click_leaves_only_at_1(plib):
    _, _, code1, _, _ = _run(plib, [SLOT0, STRAY], True)
    assert code1 == 0, code1                                        # the original: the stray press left the screen
    _, _, code0, _, _ = _run(plib, [SLOT0, STRAY], False)
    assert code0 == L.SG_E_NO_RESOURCE, code0                       # 0: the screen still waits after the stray press


def test_check_with_one_chosen_stays_at_0(plib):
    _, _, code1, _, _ = _run(plib, [SLOT0, CHECK], True)
    assert code1 == 0
    _, _, code0, _, _ = _run(plib, [SLOT0, CHECK], False)
    assert code0 == L.SG_E_NO_RESOURCE, code0


def test_check_pairs_like_the_original_exit(plib):
    """two chosen: the check press at 0 pairs them exactly as the original's stray press does (same eax, same GameState)"""
    e1, img1, c1, _, _ = _run(plib, [SLOT0, SLOT1, STRAY], True)
    e0, img0, c0, _, _ = _run(plib, [SLOT0, SLOT1, STRAY, CHECK], False)
    assert c1 == 0 and c0 == 0
    assert e0 == e1
    assert _masked(plib, img0) == _masked(plib, img1)


def test_esc_closes_without_pairing_at_0(plib):
    """the original takes Esc as the exit: with two chosen it pairs them (pairGolfersWithStory gives both hole byte +0x21 = 1, so
    two fewer golfers wait at the first tee with +0x21 = 0xff); at 0 Esc closes and pairs nobody"""
    goff = TD.objs(plib)["g_golfers"][0]
    waiting = lambda img: [g for g in range(0x98) if img[goff + 0x100 * g + 0x21] == 0xFF]
    before = waiting(_image(plib))
    e1, img1, c1, _, _ = _run(plib, [SLOT0, SLOT1], True, end_key=True)
    assert c1 == 0 and len(waiting(img1)) == len(before) - 2, (c1, waiting(img1), before)
    e0, img0, c0, _, _ = _run(plib, [SLOT0, SLOT1], False, end_key=True)
    assert c0 == 0 and e0 == 0
    assert waiting(img0) == before


def _draws(lib, clicks, hover):
    lib.defect(LEAVES, True)
    lib.defect(HOVER, hover)
    inputs, cursors = _script(clicks, end_key=True)
    with lib.state() as st:
        if st.lib.has("audio_exe.stubRequests"):
            st.call("audio_exe.stubRequests", [1])
        st.set_data_base(TD.DATA_VA)
        st.load_image(_image(lib))
        words = [w for c in inputs for w in c.args()] or [0]
        buf = (C.c_uint32 * len(words))(*words)
        st.map(TD.INPUT_VA, buf, readonly=True)
        cw = [v & 0xFFFFFFFF for p in cursors for v in p] or [0]
        cbuf = (C.c_uint32 * len(cw))(*cw)
        st.map(TD.CURSOR_VA, cbuf, readonly=True)
        st.call("dialogs.setScript", [TD.INPUT_VA, len(inputs), TD.CURSOR_VA, len(cursors)])
        st.call("commands.setTextWidth", [5, 0])
        dlog = (C.c_uint8 * LOG_CAP)()
        st.map(LOG_VA, dlog)
        st.call("gfx.attach", [])
        st.call("dialogs.recordDraw", [1])
        try:
            st.call("showGolferPairSelect", [])
        except L.SgError:
            pass
        st.call("dialogs.recordDraw", [0])
        n = st.call("dialogs.drawLog", [LOG_VA, LOG_CAP]).eax
        st.call("gfx.detach", [])
        st.call("dialogs.clearScript", [])
        st.call("commands.clearTextWidth", [])
        image = st.image()
    text = bytes(dlog[:min(n, LOG_CAP)]).decode("latin-1")
    return [ln for ln in text.split("\n") if ln], image


def test_check_lights_only_at_0(plib):
    cell4 = TD.objs(plib)["g_infoSprites821120"][0] + TD.DATA_VA + 0x2C * 4
    lit_line = f"Image_draw {cell4} "
    d1, img1 = _draws(plib, [CHECK], True)
    d0, img0 = _draws(plib, [CHECK], False)
    assert not any(ln.startswith(lit_line) for ln in d1)
    assert any(ln.startswith(lit_line) and ln.split()[3:5] == [str(0x2B5), str(0x1F6)] for ln in d0), d0[:6]
    assert img0 == img1                                              # a picture only: the GameState ends the same


def test_book_lights_only_at_0(plib):
    d1, img1 = _draws(plib, [BOOK], True)
    d0, img0 = _draws(plib, [BOOK], False)
    assert not any(ln.startswith("showGolferPairSelect.bookHover") for ln in d1)
    assert any(ln.startswith("showGolferPairSelect.bookHover") for ln in d0)
    assert img0 == img1


def test_book_press_does_not_leave_at_0(plib):
    _, _, code1, _, _ = _run(plib, [BOOK], True)
    assert code1 == 0                                               # the original: the press on the book leaves
    _, _, code0, _, _ = _run(plib, [BOOK], False)
    assert code0 == L.SG_E_NO_RESOURCE, code0
