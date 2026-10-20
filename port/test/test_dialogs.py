"""Phase 3.8 stream 38d: module `dialogs` (port/src/sim/dialogs.cpp, portsim/sim/dialogs.h, log/p38/38d_notes.md).

skillPointDialog, showCharacterIntro and drawGolferInfoPanel have no usable corpus vectors (their keys are excluded:
heap, modal input), so the evidence is the EMULATOR ORACLE of port/test/cmdemu.py (opt-in SG_EMU_ORACLE=1: the ORIGINAL
function under Unicorn over constructed GameState images, drawing stubbed, the modal pumps fed by the same scripted raw
commands the port's InputSource gets, the cursor reads answered from the same cursor sequence). The whole data image,
eax, the consumed input and the number of cursor reads must be equal. The default suite checks GOLDEN digests of fixed
cases computed from the port (pinned only after the oracle was GREEN on them: SG_EMU_ORACLE=update) and every negative
control against them.
"""
from __future__ import annotations

import json
import os
import pathlib
import random
import struct

import pytest

from port.test import lib as L
from port.test.test_commands import (DATA_VA, EH_VEC, INPUT_VA, TEXT_PUT, VIEW_TO_LOCAL, WIDGET_DROPPED, _screen_image,
                                     digest, first_diff, fnv1a, objs, put, put32)

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_dialogs.json"

pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

ENTRY = {"skillPointDialog": 0x0045F0F0, "showCharacterIntro": 0x004065C0, "drawGolferInfoPanel": 0x0045C560,
         "drawScrollText": 0x0046E260, "runScrollText": 0x0046DEA0, "scrollTextWindow": 0x0046DE70,
         "showBulletinBoard": 0x0046E810}
TEXT_VA = 0x30200000   # a writable text block (the scroll text functions write NULs into the text and put them back)
CURSOR_VA = 0x30100000
PEER_LOG_VA, PEER_LOG_CAP = 0x32000000, 4096
TITLE_VA = DATA_VA + 0x100
# dropped callees (drawing, window tree, sound, the frame Image, waits without a dispatch): stubbed with their own `ret N`
DROPPED = sorted((set(WIDGET_DROPPED) | {
    0x0040CEF0,   # drawFrameR3
    0x00404B70,   # drawCenteredText
    0x00473E60,   # Image_draw
    0x00474AE0, 0x00474DD0, 0x00475C60, 0x00474C40,   # the frame Image: ctor, create, blit thunk, dtor
    0x0045BF80,   # waitTicks (with a zero second argument it dispatches nothing)
    0x0040D320,   # drawWindow (its effect with no golfer and mode -2: none; wrapMessageBuffer's text is drawing)
    0x004740F0,   # Surface_blit2
    0x00432620,   # drawPanelBackdrop
    0x0040D0B0, 0x0046E710, 0x00404A20,   # drawFrameS2, drawRectScaled, drawTextScaled
    0x00477580,   # Widget_value (runScrollText ignores its result)
    0x00475580, 0x00475840, 0x00475C90, 0x00475210, 0x00474440,   # saveBmp, loadImage, fillRegion, loadBmpSurface, drawSprite
}) - {TEXT_PUT})
SHOW_SCREEN, GFX_FLUSH_A, PUMP_INPUT_BD0 = 0x00483BB0, 0x00497B00, 0x00483BD0
NORET = (0x00483D30, 0x00483C30, 0x00483C70, 0x00483BD0)   # enterModal483d30, pumpInput483c30, redrawAll, pumpInput483bd0
# peers scripted on both sides in the info panel's oracle (their own oracles are phase 3.5's: their story files cannot be
# opened under the emulator); eax = script_value(args, salt) for storyText, 0 for the others; the calls are compared
SCRIPTED = {"loadStoryStrings": (0x00466B70, 4), "storyText": (0x004668F0, 3), "substituteStoryNames": (0x0045C460, 1),
            "buildGolferComment": (0x00469B00, 4),
            "composeGolferSprite": (0x00462020, 1)}   # its sprite layers read heap objects; its own oracle is 3.3's


def script_value(name: str, args, salt: int) -> int:
    """the scripted peers' eax (dialogs.cpp script_value): FNV-1a over the argument dwords from the salt, bit 7"""
    if name != "storyText":
        return 0
    v = salt & 0xFFFFFFFF
    for a in args:
        v = ((v ^ (a & 0xFFFFFFFF)) * 16777619) & 0xFFFFFFFF
    return (v >> 7) & 1


class CursorSeq:
    """the oracle's View_toLocal answers: cmdemu reads [0] then [1] per call; [1] advances to the next point"""

    def __init__(self, pts):
        self.pts, self.reads = list(pts), 0

    def _cur(self):
        return self.pts[min(self.reads, len(self.pts) - 1)] if self.pts else (0, 0)

    def __getitem__(self, i):
        x, y = self._cur()
        if i == 0:
            return x
        self.reads += 1
        return y


# ------------------------------------------------------------------------------------------------ the two sides
def emu_run(name, img, args, inputs, cursors, rule, salt=None, block=None):
    import dataclasses
    from port.test import cmdemu as CE
    emu = CE.Emu()
    for a in DROPPED:
        emu.drop(a)
    for a in NORET:
        if name != "showBulletinBoard" or a != PUMP_INPUT_BD0:
            emu.stub(a, 0, 0, False)
    if name == "showBulletinBoard":
        # the board's wait is pumpInput483bd0, whose showScreen loop is where jgld dispatches the messages: its call is
        # sent to the oracle's dispatch point (frameTick483cd0, which returns 0 and so ends the showScreen loop)
        data = bytearray(emu.img.data)
        at = SHOW_SCREEN - 0x00400000
        data[at:at + 5] = b"\xE9" + struct.pack("<i", CE.FRAME_TICK - (SHOW_SCREEN + 5))
        emu.img = dataclasses.replace(emu.img, data=bytes(data))
        emu.stub(GFX_FLUSH_A, 0, 0, False)
    emu.stub(TEXT_PUT, 1, ("width",) + tuple(rule), True)
    seq = CursorSeq(cursors)
    emu.stub(VIEW_TO_LOCAL, 2, ("cursor", seq), True)
    for a, n in EH_VEC.items():
        emu.stub(a, n, 0, True)
    if salt is not None:
        for nm, (a, n) in SCRIPTED.items():
            emu.stub(a, n, (lambda nm: lambda av, ecx: script_value(nm, av, salt))(nm), False, nm)
    r = emu.run(ENTRY[name], args, img, inputs=inputs, blocks=[block] if block else ())
    r.reads = seq.reads
    return r


def port_run(lib, name, img, args, inputs, cursors, rule, salt=None, block=None):
    import ctypes as C
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        if block:
            tbuf = (C.c_uint8 * len(block[1]))(*block[1])
            st.map(block[0], tbuf)
        words = [w for c in inputs for w in c.args()] or [0]
        buf = (C.c_uint32 * len(words))(*words)
        st.map(INPUT_VA, buf, readonly=True)
        cw = [v & 0xFFFFFFFF for p in cursors for v in p] or [0]
        cbuf = (C.c_uint32 * len(cw))(*cw)
        st.map(CURSOR_VA, cbuf, readonly=True)
        st.call("dialogs.setScript", [INPUT_VA, len(inputs), CURSOR_VA, len(cursors)])
        st.call("commands.setTextWidth", [rule[0], rule[1]])
        logbuf = (C.c_uint32 * (1 + 8 * PEER_LOG_CAP))()
        st.map(PEER_LOG_VA, logbuf)
        if salt is not None:
            for nm in SCRIPTED:
                st.call("dialogs.scriptPeer", [fnv1a(nm), salt, PEER_LOG_VA, PEER_LOG_CAP])
        try:
            eax, code = st.call(name, [a & 0xFFFFFFFF for a in args]).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        stats = st.call("dialogs.scriptStats", []).eax
        st.call("dialogs.clearScript", [])
        st.call("dialogs.clearPeers", [])
        st.call("commands.clearTextWidth", [])
        n = min(logbuf[0], PEER_LOG_CAP)
        calls = [tuple(logbuf[1 + 8 * k: 1 + 8 * k + 8]) for k in range(n)]
        return eax, st.image(), code, stats & 0xFFFF, stats >> 16, calls


def compare(lib, name, case) -> str:
    img, args, inputs, cursors, rule, salt = case[:6]
    block = case[6] if len(case) > 6 else None
    r = emu_run(name, img, args, inputs, cursors, rule, salt, block)
    pe, pimg, code, used, reads, calls = port_run(lib, name, img, args, inputs, cursors, rule, salt, block)

    if r.err.startswith("NeedInput"):
        if code != L.SG_E_NO_RESOURCE:
            return f"original waits for input, port {code} {pe}"
    elif r.err == "instruction budget":
        return "" if code == L.SG_E_NO_RETURN else f"original loops, port {code} {pe}"
    elif r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    elif pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    elif name not in ("drawGolferInfoPanel", "showBulletinBoard") and (pe & 0xFFFFFFFF) != r.eax:
        return f"eax port {pe & 0xFFFFFFFF:#x} original {r.eax:#x}"
    if used != r.consumed:
        return f"input consumed port {used} original {r.consumed}"
    if reads != r.reads:
        return f"cursor reads port {reads} original {r.reads}"
    if salt is not None:
        orig = [(fnv1a(n), *[v & 0xFFFFFFFF for v in a]) for n, a in r.log if n in SCRIPTED]
        port = [(c[0], *c[1:1 + c[7]]) for c in calls]
        if orig != port:
            k = next((i for i in range(min(len(orig), len(port))) if orig[i] != port[i]), min(len(orig), len(port)))
            return f"peer calls differ at {k}: port {port[k:k + 2]} original {orig[k:k + 2]} ({len(port)} / {len(orig)})"
    return first_diff(pimg, r.image, lib)


# ------------------------------------------------------------------------------------------------ cases
def _golfer_types(lib):
    return objs(lib)["g_golferTypes"][1] // 0x230


def _dialog_image(lib, rng, golfer):
    img = bytearray(_screen_image(lib, rng))
    for nm in ("gDirtyFlagA", "gDirtyFlagB", "g_redrawPending"):
        put32(lib, img, nm, 0)
    put32(lib, img, "g_rootInput", rng.choice([0, 0, 1, 2]), 0x10)
    put32(lib, img, "g_rootInput", 0 if rng.random() < 0.85 else 1, 0x1C)
    sk = [rng.choice([0, 0, 0, 1, 2, 3, 5, 9, 10, 10, 0xF0, 0x0B]) for _ in range(10)] + [rng.getrandbits(8) for _ in range(22)]
    put(lib, img, "g_playerSkills", bytes(sk))
    put(lib, img, "g_skillMaskBlock", struct.pack("<H", rng.getrandbits(16)), 8)
    goff, _ = objs(lib)["g_golfers"]
    nt = _golfer_types(lib)
    for g in range(0x9A):
        struct.pack_into("<h", img, goff + 0x100 * g + 0xB6, rng.randrange(nt))
    if golfer >= 0:
        struct.pack_into("<h", img, goff + 0x100 * golfer + 0xB6, rng.choice([0, 0, rng.randrange(1, nt)]))
        img[goff + 0x100 * golfer + 0xF0:goff + 0x100 * golfer + 0xFA] = bytes(rng.choice([0, 1, 4, 10, 12]) for _ in range(10))
    return img


def _dialog_script(rng, xbase, n_events):
    """(inputs, cursors): clicks on the row arrows (cursor on the arrow), on OK (popup answers after it), keys"""
    from port.test import cmdemu as CE
    inputs, cursors = [], []
    for _ in range(n_events):
        k = rng.random()
        r = rng.randrange(10)
        y0 = 0x5A + 0x18 * r
        if k < 0.45:      # + arrow (sometimes on the band edges)
            pt = (xbase + 0x32 + rng.randrange(0x20), y0 + rng.choice([1, 5, 0xB, 0xC, 0xC, 0xD, 0]))
            inputs += CE.click(*pt)
            cursors.append(pt)
            if rng.random() < 0.35:   # then the - arrow of the same row (a raised row back to its entry value or 0)
                pt = (xbase + 0x32 + rng.randrange(0x20), y0 + rng.choice([0xD, 0x12, 0x18]))
                inputs += CE.click(*pt)
                cursors.append(pt)
        elif k < 0.7:     # - arrow
            pt = (xbase + 0x32 + rng.choice([0, 3, 0x1F, 0x20, -1]), y0 + rng.choice([0xD, 0x12, 0x18, 0x19]))
            inputs += CE.click(*pt)
            cursors.append(pt)
        elif k < 0.85:    # OK, then a popup answer (Down + Enter = 0: leave; Down Down Enter = 1 or Esc: stay)
            pt = (xbase + 0x15E + rng.randrange(-0x15, 0x16), 0x14E + rng.randrange(-0x15, 0x16))
            inputs += CE.click(*pt)
            cursors.append((rng.randrange(800), rng.randrange(600)))
            a = rng.random()
            if a < 0.4:
                inputs += [x for x in CE.key(0x28)[::2]] + CE.key(0x0D, 0x0D)
            elif a < 0.7:
                inputs += [x for _ in range(2) for x in CE.key(0x28)[::2]] + CE.key(0x0D, 0x0D)
            else:
                inputs += CE.key(0x1B, 0x1B)
        elif k < 0.93:
            inputs += CE.key(rng.choice([0x41, 0x0D, 0x1B]), rng.choice([0x41, 0x0D, 0]))
            cursors.append((rng.randrange(800), rng.randrange(600)))
        else:
            pt = (rng.randrange(800), rng.randrange(600))
            inputs += CE.click(*pt)
            cursors.append(pt)
    # the way out: OK, then the popup's first option (Down, Enter)
    pt = (xbase + 0x15E, 0x14E)
    for _ in range(rng.choice([1, 2])):
        inputs += CE.click(*pt) + [x for x in CE.key(0x28)[::2]] + CE.key(0x0D, 0x0D)
    cursors += [(0, 0)] * 4
    return inputs, cursors


def cases(lib, name, n, seed):
    rng = random.Random(f"{name}/{seed}")
    out = []
    for _ in range(n):
        rule = (rng.choice([5, 6, 7, 8, 9, 40]), rng.choice([0, 1, 3, 7]))
        if name == "skillPointDialog":
            golfer = rng.choice([-1, -1, rng.randrange(0x98), 0x99])
            xbase = rng.choice([0xC8, 0x9C, 0, 0x15E, -0x32, -1])
            points = rng.choice([-1, -1, 0, 1, 3, 3, 5, 0xD, 0xD, 20, -2])
            img = _dialog_image(lib, rng, golfer)
            inputs, cursors = _dialog_script(rng, xbase, rng.choice([0, 1, 3, 6, 10, 16]))
            args = [TITLE_VA, rng.getrandbits(16), points, golfer, xbase]
        elif name == "showCharacterIntro":
            img = _dialog_image(lib, rng, -1)
            off, _ = objs(lib)["g_bulletinEvents"]
            for k in range(22):
                struct.pack_into("<i", img, off + 0x28 + 0x30 * k,
                                 0 if rng.random() < 0.6 else rng.choice([1, -5, rng.getrandbits(31)]))
            inputs, cursors = _dialog_script(rng, 0xC8, rng.choice([0, 3, 10, 16, 24]))
            args = []
        elif name == "drawGolferInfoPanel":
            img, args, cursors = _panel_case(lib, rng)
            inputs = []
        elif name in ("drawScrollText", "runScrollText", "scrollTextWindow"):
            out.append(_scroll_case(lib, rng, name, rule))
            continue
        elif name == "showBulletinBoard":
            out.append(_bulletin_case(lib, rng, rule))
            continue
        else:
            raise KeyError(name)
        out.append((bytes(img), args, inputs, cursors, rule, rng.getrandbits(32)))
    return out


STORY_SPOTS = [(0x126, 0x11F), (0x167, 0x11F), (0x1A8, 0x11F), (0x1E9, 0x11F), (0x221, 0x11F)]
THOUGHT_SPOTS = [(0x13E, 0x106), (0x17F, 0x106), (0x1C0, 0x106), (0x201, 0x106), (0x253, 0x106)]


def _panel_case(lib, rng):
    """a golfer record with every field the panel reads drawn from the values that select its branches"""
    img = _dialog_image(lib, rng, -1)
    goff, _ = objs(lib)["g_golfers"]
    nt = _golfer_types(lib)
    g = rng.randrange(0x98)
    for who in (g, g ^ 1):
        R = goff + 0x100 * who
        struct.pack_into("<I", img, R + 0x10, rng.getrandbits(32))
        struct.pack_into("<h", img, R + 0x16, rng.getrandbits(15))
        img[R + 0x18] = 0 if rng.random() < 0.75 else rng.choice([1, 0x80])
        img[R + 0x19] = rng.getrandbits(8)
        img[R + 0x21] = rng.choice([-1, -1, 0, 1, 2, 5, 9, 18, 0x12, 0x13, 0x14, -2]) & 0xFF
        img[R + 0x22] = rng.choice([0, 1, 3, 7, 10, -1]) & 0xFF
        for h in range(1, 19):
            img[R + 0x23 + h] = rng.choice([0, 0, 2, 3, 4, 5, 6, 9, -3]) & 0xFF
            img[R + 0x37 + h] = rng.choice([0, 0, 1, 7])
        for k in range(10):
            img[R + 0x70 + k] = rng.choice([0, 0, 1, 5, 0x13, 0x32, rng.getrandbits(8)])
            img[R + 0x7A + k] = rng.choice([rng.randrange(220), rng.randrange(220), rng.getrandbits(8)])
            struct.pack_into("<H", img, R + 0x88 + 2 * k, rng.getrandbits(16))
        struct.pack_into("<h", img, R + 0xA0, rng.getrandbits(15))
        struct.pack_into("<h", img, R + 0xA2, rng.choice([-1, g ^ 1, g, rng.randrange(0x98)]))
        struct.pack_into("<h", img, R + 0xA4, rng.randrange(-5, 20))
        struct.pack_into("<h", img, R + 0xB0, rng.choice([-1, -1, 0, 3, 11, rng.randrange(40)]))
        struct.pack_into("<h", img, R + 0xB2, rng.choice([0, 1, 3, 4, 4, 5]))
        struct.pack_into("<h", img, R + 0xB6, rng.randrange(nt))
        struct.pack_into("<H", img, R + 0xB8, rng.getrandbits(16))
        img[R + 0xBA] = rng.choice([0, 1, 2, 3])
    put32(lib, img, "g_infoPanelToggle", rng.choice([0, 1, 1]))
    put32(lib, img, "g_infoPanelTab", rng.choice([-1, -1, 0, 1, 2, 3, 4, 5]))
    put32(lib, img, "g_infoPanelHoverFrames", rng.choice([0, 3, 5, 5, 6, 6, 7, 20]))
    put32(lib, img, "g_thoughtOnPartner", rng.choice([0, 1]))
    k = rng.random()
    if k < 0.75:
        spots = [(p, i) for i, p in zip((0, 5, 2, 3, 4), STORY_SPOTS)] + [(p, i) for i, p in enumerate(THOUGHT_SPOTS)]
        (x, y), tab = rng.choice(spots)
        if rng.random() < 0.5:   # the same button as the previous frame: the hover frame counter runs on
            put32(lib, img, "g_infoPanelTab", tab)
        j = rng.choice([0, 5, 0x18, 0x19, 0x1A, 40])
        cur = (x + rng.randrange(-j, j + 1), y + rng.randrange(-j, j + 1))
        if rng.random() < 0.2:   # exactly on a button's radius
            dx, dy = rng.choice([(0x19, 0), (0, 0x19), (-0x19, 0), (0x18, 0), (0, -0x18)])
            cur = (x + dx, y + dy)
    else:
        cur = (rng.randrange(800), rng.randrange(600))
    return img, [g, rng.choice([0, 1, 1])], [cur]


def _scroll_text(rng) -> bytes:
    """a menu-like text: captions, entries (a leading ' ' or '_' and a hotkey letter), '|' columns, newlines"""
    lines = []
    many = rng.random() < 0.08   # more than 32 entries: the hotkey table keeps the first 32
    for _ in range(rng.choice([0, 1, 2, 3, 5, 8, 12, 20, 40]) if not many else 40):
        k = rng.random()
        word = bytes(rng.choice(b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJ0123456789 .,") for _ in range(rng.randrange(0, 18)))
        if k < 0.55 or many:
            word = rng.choice(b" _").to_bytes(1, "little") + (word[:8] if many else word)
        if rng.random() < 0.25:
            word += b"|" + bytes(rng.choice(b"0123456789$") for _ in range(rng.randrange(0, 6)))
        lines.append(word)
    text = b"\n".join(lines)
    if lines and rng.random() < 0.8:
        text += b"\n"
    return text[:0x3F0]


def _scroll_image(lib, rng):
    img = _dialog_image(lib, rng, -1)
    put32(lib, img, "g_screenWidth", rng.choice([640, 800, 800, 1024]))
    put32(lib, img, "g_scrollTextWin", rng.choice([-1, -1, 0x30, 0x50, 0x9C, 0]), 0x10)
    put32(lib, img, "g_scrollTextWin", rng.choice([5, 6, 0x10, 0x31, 0x50, 0xE0, 0x100]), 0x14)
    put32(lib, img, "g_scrollTextWin", rng.choice([-1, 0, 2]), 0x2C)
    put32(lib, img, "g_scrollTextWin", rng.choice([0, 0, 1]), 0x40)
    put32(lib, img, "g_scrollTextWin", rng.choice([0, 0, 0, 1]), 0x24)
    put32(lib, img, "g_scrollTextWin", rng.choice([0, 1, 1]), 0x28)
    put32(lib, img, "g_scrollTextWin", rng.choice([0, 0, 2, 5, rng.getrandbits(32)]), 0x30)
    put32(lib, img, "g_scrollTextState", rng.choice([0, 0, 0, 1]), 4)
    put32(lib, img, "g_scrollTextState", rng.choice([0, 0, 1, 6, rng.getrandbits(32)]))   # the entries' colour bits
    put32(lib, img, "g_scrollTextVars", 1 if rng.random() < 0.15 else 0)
    put32(lib, img, "f_w_4e3da4", rng.choice([-1, -1, 0, 1, 3, 31]))
    put32(lib, img, "f_w_4e3da4", rng.choice([7, 7, 9, 0]) if rng.random() < 0.05 else 7, 8)
    put32(lib, img, "g_flag_83afcc", 1 if rng.random() < 0.05 else 0)
    return img


def _scroll_script(rng, img, lib):
    """clicks and cursor points on the entry lines (inverting the scaling the loop applies), keys"""
    from port.test import cmdemu as CE
    o = objs(lib)
    w = struct.unpack_from("<i", img, o["g_screenWidth"][0])[0]
    s2, s4 = (w * 2 + 160) // 320, (w * 4 + 160) // 320
    top = struct.unpack_from("<i", img, o["g_scrollTextWin"][0] + 0x14)[0]
    x = struct.unpack_from("<i", img, o["g_scrollTextWin"][0] + 0x10)[0]
    x = 0x60 if x == -1 else x
    inputs, cursors = [], []
    for _ in range(rng.choice([0, 1, 2, 4, 8])):
        k = rng.random()
        line = rng.randrange(-1, 12) if rng.random() < 0.9 else rng.randrange(28, 36)
        tx, ty = x + rng.randrange(-4, 0x60), top + 4 + 7 * line + rng.randrange(7)
        if k < 0.3:
            inputs += CE.click(tx * s2 // 2, ty * s2 // 2)
        elif k < 0.35:   # a right click: onPoint2, onPress2, onUnpoint2
            inputs += [CE.Raw(5, tx * s2 // 2, ty * s2 // 2), CE.Raw(4, tx * s2 // 2, ty * s2 // 2), CE.Raw(6)]
        elif k < 0.75:
            inputs += CE.key(rng.choice([0x28, 0x28, 0x26, 0x0D, 0x20, 0x1B, 0x41, 0x43, 0x5A]), rng.choice([0, 0x41]))
        else:
            inputs += [CE.Raw(2, tx, ty)]   # a move: onPoint (no button), the hover follows the cursor
        cursors.append((tx * s4 // 4, ty * s4 // 4))
    inputs += CE.key(0x1B, 0x1B) if rng.random() < 0.5 else CE.key(0x0D, 0x0D)
    inputs += CE.key(0x1B, 0x1B)
    cursors += [(rng.randrange(800), rng.randrange(600)) for _ in range(3)]
    return inputs, cursors


def _scroll_case(lib, rng, name, rule):
    img = _scroll_image(lib, rng)
    text = _scroll_text(rng)
    block = (TEXT_VA, (bytes(0x10) + text).ljust(0x1000, b"\0"))   # one page: the emulator maps whole pages
    va = TEXT_VA + 0x10
    if name == "drawScrollText":
        args, inputs, cursors = [va, rng.choice([-1, 0, 1, 2, 5])], [], []
    else:
        inputs, cursors = _scroll_script(rng, img, lib)
        args = [va, 1] if name == "runScrollText" else [va, rng.choice([-1, 0x50, 0x9C, 0x30]), rng.choice([5, 6, 0x31, 0x50])]
    return (bytes(img), args, inputs, cursors, rule, None, block)


def _bulletin_case(lib, rng, rule):
    from port.test import cmdemu as CE
    img = _dialog_image(lib, rng, -1)
    o = objs(lib)
    put32(lib, img, "f_w_4e3db8", rng.choice([-1, 0x63, 0, 3, 7, 21]))
    put32(lib, img, "f_w_4e3db8", rng.choice([-1, -1, rng.randrange(0, 50 << 10)]), 4)
    put32(lib, img, "g_scrollTextWin", rng.randrange(0, 50 << 10), 0x54)
    put32(lib, img, "g_bulletinAnimTick", rng.choice([0, 0x12, 0x13, 0x13, 0x30]))
    if rng.random() < 0.4:   # the camera on the photo's tile: worldToScreen sees it
        put32(lib, img, "gCameraTileX", struct.unpack_from("<i", img, o["f_w_4e3db8"][0] + 4)[0] >> 10)
        put32(lib, img, "gCameraTileY", struct.unpack_from("<i", img, o["g_scrollTextWin"][0] + 0x54)[0] >> 10)
        put32(lib, img, "g_screenHeight", 600)
        put32(lib, img, "g_zoomScale", rng.choice([1, 2, 4]))
        put32(lib, img, "g_cameraRot", rng.choice([0, 2, 4, 6]))
    put32(lib, img, "g_difficulty", rng.choice([0, 1, 2, 3]))
    off = o["g_bulletinEvents"][0]
    none, full = rng.random() < 0.1, rng.random() < 0.1   # no dated event / every record dated
    for k in range(22):
        date = 0 if none or (rng.random() < 0.5 and not full) else rng.choice([1, 0x3FF, 0x400, 0x2000 * 3 + 0x155, rng.getrandbits(20), -5])
        struct.pack_into("<ii", img, off + 0x28 + 0x30 * k, date, rng.choice([0, 1, 2, 5]))
    put32(lib, img, "g_date", rng.randrange(0, 50000))
    put(lib, img, "g_skillMaskBlock", struct.pack("<h", rng.choice([0, 0, 1, 0x22, -1])), 8)
    struct.pack_into("<h", img, o["g_tileBuildCode"][0] + 0xB6, rng.randrange(5))
    put32(lib, img, "g_screenWidth", rng.choice([640, 800, 800, 1024]))
    inputs = CE.click(rng.randrange(800), rng.randrange(600)) if rng.random() < 0.7 else CE.key(0x1B, 0x1B)
    more, cursors = _dialog_script(rng, 0xC8, rng.choice([0, 2, 5]))
    return (bytes(img), [], inputs + more, [(0, 0)] + cursors, rule, rng.getrandbits(32), None)


SCROLL = ["drawScrollText", "runScrollText", "scrollTextWindow", "showBulletinBoard"]
DIALOGS = ["skillPointDialog", "showCharacterIntro", "drawGolferInfoPanel"] + SCROLL
ORACLE_N = {"drawGolferInfoPanel": 400, "drawScrollText": 300, "runScrollText": 200, "showBulletinBoard": 200}


@need_exe
@emu_oracle
@pytest.mark.parametrize("name", DIALOGS)
def test_dialog_equals_emulated_original(portlib, name):
    bad = []
    for k, case in enumerate(cases(portlib, name, ORACLE_N.get(name, 150), 1)):
        d = compare(portlib, name, case)
        if d:
            bad.append((k, d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


# ------------------------------------------------------------------------------------------------ golden digests
GOLDEN_N = {"skillPointDialog": 120, "showCharacterIntro": 60, "drawGolferInfoPanel": 200, "drawScrollText": 100,
            "runScrollText": 120, "scrollTextWindow": 60, "showBulletinBoard": 120}


def _golden_cases(lib, name):
    key = ("golden", name)
    if key not in _CACHE:
        _CACHE[key] = cases(lib, name, GOLDEN_N[name], "golden")
    return _CACHE[key]


_CACHE = {}


def _port_digest(lib, name, case) -> str:
    img, args, inputs, cursors, rule, salt = case[:6]
    block = case[6] if len(case) > 6 else None
    pe, pimg, code, used, reads, calls = port_run(lib, name, img, args, inputs, cursors, rule, salt, block)
    if name in ("drawGolferInfoPanel", "showBulletinBoard") and pe is not None:
        pe = 0   # void: eax is not part of the result
    h = digest(pe, pimg)
    tail = json.dumps([code, used, reads, [list(c) for c in calls]])
    import hashlib
    return h + ":" + hashlib.sha256(tail.encode()).hexdigest()[:12]


def _golden_port(lib, name):
    return [_port_digest(lib, name, c) for c in _golden_cases(lib, name)]


def _golden_load():
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


@need_exe
@pytest.mark.parametrize("name", DIALOGS)
def test_golden_digests(portlib, name):
    """the port's results for fixed cases equal the digests pinned after the oracle was GREEN on these very cases"""
    got = _golden_port(portlib, name)
    if os.environ.get("SG_EMU_ORACLE") == "update":
        bad = [(k, d) for k, c in enumerate(_golden_cases(portlib, name)) if (d := compare(portlib, name, c))]
        assert not bad, f"oracle not GREEN on the golden cases: {bad[:4]}"
        g = _golden_load()
        g[name] = got
        GOLDEN.write_text(json.dumps(g, indent=0, sort_keys=True))
    want = _golden_load().get(name)
    assert want is not None, "no golden digests: run with SG_EMU_ORACLE=update once the oracle is GREEN"
    bad = [k for k, (a, b) in enumerate(zip(got, want)) if a != b]
    assert len(got) == len(want) and not bad, f"{len(bad)} cases differ, first {bad[:5]}"


# ------------------------------------------------------------------------------------------------ negative controls
NEGCTL_RED = [("skillPointDialog", m) for m in (1, 2, 3, 4)] + [("showCharacterIntro", m) for m in (1, 2)] + \
             [("drawGolferInfoPanel", m) for m in (1, 2, 3, 4, 5)] +              [("drawScrollText", 1), ("drawScrollText", 2), ("runScrollText", 3), ("scrollTextWindow", 1)] +              [("showBulletinBoard", 1), ("showBulletinBoard", 2)]


@need_exe
@pytest.mark.parametrize("name,mode", NEGCTL_RED)
def test_negctl_reads_red(portlib, name, mode):
    """every wrong body changes at least one golden case (and, with the oracle, differs from the original there)"""
    want = _golden_load().get(name)
    assert want is not None
    portlib.negctl(name, mode)
    try:
        got = _golden_port(portlib, name)
        red = [k for k, (a, b) in enumerate(zip(got, want)) if a != b]
        if red and os.environ.get("SG_EMU_ORACLE") == "1":
            assert compare(portlib, name, _golden_cases(portlib, name)[red[0]]), "red against the digest, not the original"
    finally:
        portlib.negctl_reset()
    assert red, f"{name} negctl {mode} is blind on the golden cases"


def test_every_dialogs_function_has_a_negctl(portlib):
    mine = {n for n, f in portlib.functions().items() if f.get("module") == "dialogs" and not n.startswith("dialogs.")}
    assert mine == {n for n, _ in NEGCTL_RED}, sorted(mine)


@need_exe
def test_scroll_text_needs_text_metrics(portlib):
    """the layout measures every character with Text_put: without a TextMetrics provider it stops (SG_E_NO_RESOURCE)
    before its first width; an empty text measures nothing"""
    import ctypes as C
    rng = random.Random(4)
    img = _scroll_image(portlib, rng)
    for text, code in ((b" Yes\n No\n", L.SG_E_NO_RESOURCE), (b"", 0)):
        with portlib.state() as st:
            st.set_data_base(DATA_VA)
            st.load_image(img)
            data = (bytes(0x10) + text).ljust(0x1000, b"\0")
            tbuf = (C.c_uint8 * len(data))(*data)
            st.map(TEXT_VA, tbuf)
            try:
                st.call("drawScrollText", [TEXT_VA + 0x10, 0])
                got = 0
            except L.SgError as e:
                got = e.code
            assert got == code


@need_exe
def test_info_panel_reads_the_cursor_from_the_input_source(portlib):
    """drawGolferInfoPanel's hover depends on the cursor: with no InputSource it stops (NeedInput = SG_E_NO_RESOURCE)
    before its hover writes, as every straddler that needs the cursor does (U-0943)"""
    rng = random.Random(7)
    for _ in range(20):
        img, args, _ = _panel_case(portlib, rng)
        g = args[0]
        goff, _ = objs(portlib)["g_golfers"]
        b = bytearray(img)
        b[goff + 0x100 * g + 0x21] = 3   # a current hole: the panel reaches the hover
        b[goff + 0x100 * g + 0x22] = 1   # a lie with a name
        b[goff + 0x100 * g + 0x18] = 0
        b[goff + 0x100 * g + 0xB0:goff + 0x100 * g + 0xB2] = struct.pack("<h", -1)   # the thoughts page: no story file read
        with portlib.state() as st:
            st.set_data_base(DATA_VA)
            st.load_image(bytes(b))
            with pytest.raises(L.SgError) as e:
                st.call("drawGolferInfoPanel", args)
            assert e.value.code == L.SG_E_NO_RESOURCE
