"""Phase 3.8 stream 38h: module `chareditor` (port/src/sim/chareditor.cpp, portsim/sim/chareditor.h, log/p38/38h_notes.md).

showCharacterEditor, pickCharEditor, nearestCharControl and charEditorZone have no usable corpus vectors (the editor's
keys are excluded: heap, modal input; the other three have none), so the evidence is the EMULATOR ORACLE of
port/test/cmdemu.py (opt-in SG_EMU_ORACLE=1): the ORIGINAL function under Unicorn over constructed GameState images,
drawing stubbed, the modal pumps fed by the same scripted raw commands the port's InputSource gets, the cursor reads
answered from the same cursor sequence. Unlike stream 38d's oracle, View_toLocal runs for real (with View::fromParent)
over a fake display whose cursor slot serves the sequence, because the port's editor calls the registered
View::fromParent too. The editor's two release-wait pumps are DRAINS in the oracle (nothing has arrived there); the
port's test source refuses those pumps the same way. Peers whose own evidence is elsewhere and which read files or heap
objects are scripted on both sides and their calls compared (composeGolferSprite, buildGolferComment, listMatchingFiles,
loadThemeFile, promptAndSaveFile). The whole data image, eax, the consumed input and the number of cursor reads must be
equal. The default suite checks GOLDEN digests of fixed cases computed from the port (pinned only after the oracle was
GREEN on them: SG_EMU_ORACLE=update) and every negative control against them.
"""
from __future__ import annotations

import json
import os
import pathlib
import random
import struct

import pytest

from port.test import lib as L
from port.test.test_commands import DATA_VA, EH_VEC, INPUT_VA, TEXT_PUT, digest, first_diff, fnv1a, objs, put, put32
from port.test.test_dialogs import DROPPED as DIALOG_DROPPED
from port.test.test_dialogs import NORET, CursorSeq, _dialog_image

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_chareditor.json"

pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

ENTRY = {"showCharacterEditor": 0x004385D0, "pickCharEditor": 0x00438390, "nearestCharControl": 0x00438260,
         "charEditorZone": 0x004382F0}
CURSOR_VA = 0x30100000
PEER_LOG_VA, PEER_LOG_CAP = 0x32000000, 8192
FAKE_DISPLAY = 0x31000000          # the display object: vtable at +0x100, stubs at +0x800..
SLOT_STUB, CURSOR_STUB, SLOT9C_STUB, SLOT38_STUB = (FAKE_DISPLAY + 0x800, FAKE_DISPLAY + 0x810, FAKE_DISPLAY + 0x820,
                                                    FAKE_DISPLAY + 0x830)
# composeGolferSprite's compositor object (its vtable slot 0x14 places a layer: thiscall, three arguments); the part
# objects are NULL, so their slot 0x10 calls are skipped: the original then runs only its look choice and write-back
FAKE_COMP, COMP_STUB = FAKE_DISPLAY + 0x500, FAKE_DISPLAY + 0x840
COMPOSITOR_VA, PART_TABLES = 0x0081CA14, [(0x0081CA6C + 88 * k) for k in range(0xA0)] + [0x0081FA34, 0x0081FA8C]
# the editor's own dropped callees beyond the dialogs' list: startSoundSlot, drawMessageBuffer (it NULs and puts back),
# Palette::nearest (bio only)
DROPPED = sorted(set(DIALOG_DROPPED) | {0x00448200, 0x0045B0D0, 0x00483420})
RELEASE_DRAINS = {0x0043A23A: "release wait 1", 0x0043A254: "release wait 2"}   # return addresses of their pumpMessages calls
BIO_EDITBOX = 0x00486110          # EditBox::create: the bio entry the port does not take
SCRIPTED = {"composeGolferSprite": (0x00462020, 1), "buildGolferComment": (0x00469B00, 4),
            "listMatchingFiles": (0x0043D2A0, 3), "loadThemeFile": (0x00437FA0, 3), "promptAndSaveFile": (0x00437910, 3)}
del SCRIPTED["composeGolferSprite"]   # reached through addSprite too: it runs for real on both sides (FAKE_COMP)


def script_value(name: str, args, salt: int) -> int:
    """the scripted peers' eax (chareditor.cpp script_value)"""
    v = salt & 0xFFFFFFFF
    for a in args:
        v = ((v ^ (a & 0xFFFFFFFF)) * 16777619) & 0xFFFFFFFF
    if name == "listMatchingFiles":
        return (v >> 7) & 3
    if name == "promptAndSaveFile":
        return (v >> 7) & 1
    return 0


def _display_block():
    vt = bytearray(0x400)
    for k in range(0x100):
        struct.pack_into("<I", vt, 4 * k, SLOT_STUB)
    struct.pack_into("<I", vt, 0x2C, CURSOR_STUB)
    struct.pack_into("<I", vt, 0x9C, SLOT9C_STUB)
    struct.pack_into("<I", vt, 0x38, SLOT38_STUB)
    blk = bytearray((struct.pack("<I", FAKE_DISPLAY + 0x100) + bytes(0xFC) + bytes(vt)).ljust(0x1000, b"\xC3"))
    struct.pack_into("<I", blk, FAKE_COMP - FAKE_DISPLAY, FAKE_DISPLAY + 0x600)
    for k in range(0x40):
        struct.pack_into("<I", blk, 0x600 + 4 * k, COMP_STUB)
    return (FAKE_DISPLAY, bytes(blk))


# ------------------------------------------------------------------------------------------------ the two sides
def emu_run(name, img, args, inputs, cursors, rule, salt=None, cover=None):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    for a in DROPPED:
        emu.drop(a)
    for a in NORET:
        emu.stub(a, 0, 0, False)
    emu.stub(TEXT_PUT, 1, ("width",) + tuple(rule), True)
    seq = CursorSeq(cursors)
    emu.stub(CURSOR_STUB, 2, ("cursor", seq), True, "display cursor")
    emu.stub(SLOT_STUB, 0, 0, False, "display slot")
    emu.stub(SLOT9C_STUB, 3, 0, True, "display slot 0x9c")
    emu.stub(SLOT38_STUB, 1, 0, True, "display slot 0x38")
    emu.stub(COMP_STUB, 3, 0, True, "compositor slot")

    def bio(uc, a, ecx):
        raise RuntimeError("bio EditBox")
    emu.stub_py(BIO_EDITBOX, 6, bio, True, "EditBox::create")
    for a, n in EH_VEC.items():
        emu.stub(a, n, 0, True)
    if salt is not None:
        for nm, (a, n) in SCRIPTED.items():
            emu.stub(a, n, (lambda nm: lambda av, ecx: script_value(nm, av, salt))(nm), False, nm)
    saved = dict(CE.DRAIN_CALLERS)
    CE.DRAIN_CALLERS.update(RELEASE_DRAINS)
    try:
        r = emu.run(ENTRY[name], args, img, inputs=inputs, blocks=[_display_block()], cover=cover)
    finally:
        CE.DRAIN_CALLERS.clear()
        CE.DRAIN_CALLERS.update(saved)
    r.reads = seq.reads
    return r


def port_run(lib, name, img, args, inputs, cursors, rule, salt=None):
    import ctypes as C
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        words = [w for c in inputs for w in c.args()] or [0]
        buf = (C.c_uint32 * len(words))(*words)
        st.map(INPUT_VA, buf, readonly=True)
        cw = [v & 0xFFFFFFFF for p in cursors for v in p] or [0]
        cbuf = (C.c_uint32 * len(cw))(*cw)
        st.map(CURSOR_VA, cbuf, readonly=True)
        st.call("chareditor.setScript", [INPUT_VA, len(inputs), CURSOR_VA, len(cursors)])
        st.call("commands.setTextWidth", [rule[0], rule[1]])
        logbuf = (C.c_uint32 * (1 + 8 * PEER_LOG_CAP))()
        st.map(PEER_LOG_VA, logbuf)
        if salt is not None:
            for nm in SCRIPTED:
                st.call("chareditor.scriptPeer", [fnv1a(nm), salt, PEER_LOG_VA, PEER_LOG_CAP])
        try:
            eax, code = st.call(name, [a & 0xFFFFFFFF for a in args]).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        stats = st.call("chareditor.scriptStats", []).eax
        st.call("chareditor.clearScript", [])
        st.call("chareditor.clearPeers", [])
        st.call("commands.clearTextWidth", [])
        n = min(logbuf[0], PEER_LOG_CAP)
        calls = [tuple(logbuf[1 + 8 * k: 1 + 8 * k + 8]) for k in range(n)]
        return eax, st.image(), code, stats & 0xFFFF, stats >> 16, calls


def compare(lib, name, case) -> str:
    img, args, inputs, cursors, rule, salt = case
    r = emu_run(name, img, args, inputs, cursors, rule, salt)
    pe, pimg, code, used, reads, calls = port_run(lib, name, img, args, inputs, cursors, rule, salt)
    if "bio EditBox" in r.err:
        return "" if code == L.SG_E_NO_RESOURCE else f"original reaches the bio EditBox, port {code} {pe}"
    if r.err.startswith("NeedInput"):
        if code != L.SG_E_NO_RESOURCE:
            return f"original waits for input, port {code} {pe}"
    elif r.err == "instruction budget":
        return "" if code == L.SG_E_NO_RETURN else f"original loops, port {code} {pe}"
    elif r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    elif pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    elif (pe & 0xFFFFFFFF) != r.eax:
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
def _spots(lib, img):
    off, sz = objs(lib)["f_tab_point16_4c7b38"]
    pts = [struct.unpack_from("<hh", img, off + 4 * i) for i in range(sz // 4)]
    return pts[:next(i for i, p in enumerate(pts) if p[0] == 0)]


def _zones(lib, img):
    off, sz = objs(lib)["f_tab_pair32_4c7be0"]
    return [(x + 0x3C, y + 0x3C) for x, y in (struct.unpack_from("<ii", img, off + 8 * i) for i in range(10))] + \
        [(0x30A, 0x16B), (0x12, 0x1EF)]


def _text(rng, n):
    return bytes(rng.choice(b"abcdefghijklmnopqrstuvwxyz ABCXYZ0129") for _ in range(rng.randrange(0, n)))


def _editor_image(lib, rng, character, golfer, member):
    img = bytearray(_dialog_image(lib, rng, -1))
    o = objs(lib)
    put32(lib, img, "g_graphicsDevice", FAKE_DISPLAY)
    put32(lib, img, "g_viewTreeRoot", 0)
    for at in (0x9C, 0x130, 0x1AC, 0x1B0, 0x1BC, 0x1C0):
        put32(lib, img, "g_rootWindow", 0, at)
    struct.pack_into("<I", img, COMPOSITOR_VA - DATA_VA, FAKE_COMP)
    for va in PART_TABLES:
        struct.pack_into("<I", img, va - DATA_VA, 0)
    for nm in ("gDirtyFlagA", "gDirtyFlagB", "g_redrawPending"):
        put32(lib, img, nm, 0)
    goff = o["g_golfers"][0]
    nt = o["g_golferTypes"][1] // 0x230
    for g in {golfer, 0x99}:
        R = goff + 0x100 * g
        img[R + 0x18] = (0x20 | rng.getrandbits(5)) if (member and g == golfer) else rng.choice([0, 0x40, 0x60, 0xE0, 0x07])
        img[R + 0x19] = rng.getrandbits(8)
        struct.pack_into("<h", img, R + 0xB6, rng.choice([0, rng.randrange(nt)]))
        struct.pack_into("<H", img, R + 0xB8, rng.getrandbits(16))
        img[R + 0xBA] = rng.randrange(3)
        img[R + 0xBB] = rng.choice([0, 1, 7, 99, rng.getrandbits(8)])
        img[R + 0xF0:R + 0xFC] = bytes(rng.choice([0, 1, 3, 9, 10, 10, 11, 0xFF]) for _ in range(12))
    if member and golfer != 0x99 and rng.random() < 0.3:
        img[goff + 0x100 * 0x99 + 0x18] = 0x20   # a preview golfer of the member class: the pro look sets the sprites
    toff = o["g_golferTypes"][0]
    for t in {character, rng.randrange(nt), struct.unpack_from("<h", img, goff + 0x100 * 0x99 + 0xB6)[0]}:
        T = toff + 0x230 * t
        img[T:T + 16] = (_text(rng, 12) + b"\0").ljust(16, b"\0")
        img[T + 16:T + 32] = (_text(rng, 12) + b"\0").ljust(16, b"\0")
        img[T + 0x20:T + 0x2C] = rng.randbytes(12)
        img[T + 0x22] = rng.choice([0, 3, 18, 19, 20, 0xFE, rng.getrandbits(8)])
        img[T + 0x21] = rng.choice([img[T + 0x21], img[T + 0x21], 0x80, 0x87, 0x40, 0x00])   # empty bit fields too
        img[T + 0x23] = rng.choice([0, 0x35, 0xC7, rng.getrandbits(8)])
        struct.pack_into("<I", img, T + 0x2C, rng.choice([0, 0, 8, 0x80, rng.getrandbits(32)]))
        img[T + 0x30:T + 0x230] = (_text(rng, 80) + b"\0").ljust(0x200, b"\0")
    soff = o["g_golferTypeState"][0] + 0x2C * character
    img[soff:soff + 0x2C] = rng.randbytes(0x2C)
    img[soff + 0x29] = rng.choice([0, 0xFF, 3])
    if rng.random() < 0.3:
        img[soff] = 0
    if rng.random() < 0.3:
        img[soff + 0x2A:soff + 0x2C] = bytes(2)
    poff = o["g_golferPhrases"][0] + 0x4E2 * character
    for k in range(25):
        img[poff + 0x32 * k:poff + 0x32 * (k + 1)] = (b"" if rng.random() < 0.4 else _text(rng, 30) + b"\0").ljust(0x32, b"\0")
    put32(lib, img, "g_commentTone", rng.choice([0x80006318, 0, rng.getrandbits(32)]))
    aoff, asz = o["g_thoughtArgByKind"]
    img[aoff:aoff + asz] = rng.randbytes(asz)
    put(lib, img, "g_theme", (_text(rng, 10) + b"\0").ljust(16, b"\0"))
    foff = o["g_fileNames"][0]
    for k in range(4):
        img[foff + 100 * k:foff + 100 * (k + 1)] = (_text(rng, 14) + b".chr\0").ljust(100, b"\0")
    put32(lib, img, "g_loopCounter59b76c", rng.choice([0, 5, 10, 25, 80, 80]), 0)
    put32(lib, img, "g_loopCounter59b76c", rng.choice([0, 7, 10, 40, 90, 90]), 4)
    if rng.random() < 0.04:   # no display: View_toLocal writes no cursor (the frame slots keep their values)
        put32(lib, img, "g_graphicsDevice", 0)
    put32(lib, img, "g_ui_5aa554", rng.choice([0, 3, 9, 10, 12]))
    spots = _spots(lib, img)
    put32(lib, img, "g_panelState", rng.choice([-1, rng.randrange(len(spots))]), 4)
    return img


def typed(ch, code=None):
    """one typed key as a modal text loop sees it: an idle pump first (a WM_KEYDOWN with code 0: flushRedraw clears the
    previous key's pending flag there, as the real game's empty pumps do), then the WM_CHAR (onKey: char, code)"""
    from port.test import cmdemu as CE
    return [CE.Raw(8, 0, 0), CE.Raw(7, ch, ch if code is None else code)]


def _tib_keys(rng):
    """textInputBox input: a few characters, sometimes a backspace or a cursor key, then Enter (or Esc)"""
    keys = []
    for _ in range(rng.choice([0, 0, 1, 3, 6])):
        k = rng.random()
        keys += typed(rng.choice(b"abcxyzABC019 ")) if k < 0.8 else typed(8) if k < 0.9 else typed(0, rng.choice([0x25, 0x24]))
    keys += typed(0x0D) if rng.random() < 0.85 else typed(0x1B)
    return keys


def _picker_actions(rng, zones):
    """pickCharEditor input: clicks on zones (presets, arrows), a right click, a press above the picker"""
    from port.test import cmdemu as CE
    inputs, cursors = [], []
    for i in range(rng.choice([1, 1, 2, 3, 5])):
        k = rng.random() if i else rng.choice([rng.random(), 0.1])
        if k < 0.25:      # an arrow, then more
            pt = zones[rng.choice([10, 11, 10])]
        elif k < 0.75:    # a preset
            pt = zones[rng.randrange(10)]
        elif k < 0.85:    # above the picker
            pt = (rng.randrange(800), rng.randrange(0, 0x102))
        else:
            pt = (rng.randrange(800), rng.randrange(0x102, 600))
        j = rng.choice([0, 0, 10, 40])
        pt = (pt[0] + rng.randrange(-j, j + 1), pt[1] + rng.randrange(-j, j + 1))
        cursors.append(pt)
        if rng.random() < 0.12:   # a right click: onPoint2, onPress2, onUnpoint2
            inputs += [CE.Raw(5, *pt), CE.Raw(4, *pt), CE.Raw(6)]
        else:
            inputs += CE.click(*pt)
        if k < 0.75 and 0.25 <= k:
            break             # a preset ends the picker
    return inputs, cursors


def _editor_script(lib, rng, img, character, golfer, member, salt):
    """(inputs, cursors): one cursor point and one batch per pass of the editor, with the nested dialogs' input after
    the press that opens them"""
    from port.test import cmdemu as CE
    spots, zones = _spots(lib, img), _zones(lib, img)
    o = objs(lib)
    tb, s4e = DATA_VA + o["g_textBuffer"][0], DATA_VA + o["g_str4e9a84"][0]
    inputs, cursors = [], []

    def press(pt, right=False):
        cursors.append(pt)
        inputs.extend([CE.Raw(5, *pt), CE.Raw(4, *pt), CE.Raw(6)] if right else CE.click(*pt))

    for _ in range(rng.choice([0, 1, 3, 6, 10, 16])):
        k = rng.random()
        right = rng.random() < 0.3
        if k < 0.45:
            c = rng.choice([i for i in range(len(spots)) if i != 19 and (i != 20 or member)] + [12, 12, 13, 14, 15, 16, 16, 8, 8, 10, 10, 10])
            j = rng.choice([0, 0, 0, 3, 12])
            x, y = spots[c]
            press((x + rng.randrange(-j, j + 1), y + rng.randrange(-j, j + 1)), right)
            if c == 8 and script_value("listMatchingFiles", [tb, s4e, s4e], salt):
                if rng.random() < 0.3:
                    inputs.extend(typed(0x1B))
                else:
                    for _ in range(rng.randrange(4)):
                        inputs.extend(typed(0, 0x28))
                    inputs.extend(typed(0x0D))
                cursors.append((0, 0))
            elif c == 9 and script_value("promptAndSaveFile", [character, golfer if member else -1, 0], salt):
                inputs.extend(typed(0x0D) if rng.random() < 0.6 else typed(0x1B))
                cursors.append((0, 0))
            elif c == 12:
                pi, pc = _picker_actions(rng, zones)
                inputs.extend(pi)
                cursors.extend(pc)
        elif k < 0.6:     # hover on one control for a while (the tip after 10 passes): key-down batches
            pt = spots[rng.choice([rng.randrange(len(spots)), 20, 8, 19])]
            for _ in range(rng.choice([2, 11, 13])):
                cursors.append(pt)
                inputs.append(CE.Raw(8, rng.choice([0x41, 0x26, 0x10]), 0))
        elif k < 0.75:    # the name area: name, title, demographics
            line = rng.choice([0, 1, 2, 3, 3, 2, 4])
            pt = (0x9A + rng.randrange(-0x4F, 0x50), 0x18 + 0x14 * line + rng.randrange(0x14))
            press(pt, right)
            if line in (0, 1):
                inputs.extend(_tib_keys(rng))
        elif k < 0.88:    # a comment line
            pt = (0x242 + rng.randrange(-0xB1, 0xB2), 0x10E + rng.choice([0, 5, 0x10, 0x55, 0x12F, 0x13F, 0x148]))
            press(pt, right)
            inputs.extend(_tib_keys(rng))
        elif k < 0.95:    # a skill bar
            pt = (0x208 + rng.randrange(-0x18, 0x19), rng.choice([rng.randrange(0x3E, 0xDE), 0x3D, 0xDE, 0xE0, 0x3E, 0xDD]))
            press(pt, right or rng.random() < 0.3)
        else:
            press((rng.randrange(800), rng.randrange(600)), right)
    if rng.random() < 0.15:   # Cancel before leaving
        press(spots[18])
    if rng.random() < 0.9:    # Exit (sometimes after hovering on it: the tip threshold on the last pass)
        if rng.random() < 0.3:
            for _ in range(rng.choice([9, 10, 11, 12])):
                cursors.append(spots[19])
                inputs.append(CE.Raw(8, 0x41, 0))
        press(spots[19], rng.random() < 0.2)
    return inputs, cursors or [(0, 0)]   # (an empty list would make the port's test source refuse the first read)


def cases(lib, name, n, seed):
    rng = random.Random(f"{name}/{seed}")
    out = []
    nt = objs(lib)["g_golferTypes"][1] // 0x230
    for _ in range(n):
        rule = (rng.choice([5, 6, 7, 8, 9]), rng.choice([0, 1, 3, 7]))
        salt = rng.getrandbits(32)
        if name == "showCharacterEditor":
            character = rng.choice([0, 0, rng.randrange(nt), rng.randrange(nt), 83])
            golfer = rng.choice([rng.randrange(0x98), rng.randrange(0x98), 0x99])
            member = rng.random() < 0.5
            img = _editor_image(lib, rng, character, golfer, member)
            inputs, cursors = _editor_script(lib, rng, img, character, golfer, member, salt)
            out.append((bytes(img), [character, golfer], inputs, cursors, rule, salt))
        elif name == "pickCharEditor":
            img = _editor_image(lib, rng, 0, 0, False)
            put32(lib, img, "g_ui_5aa55c", rng.choice([0, 0, 10, 0x3C, 0x46, 0x50, 5, -10]))
            if rng.random() < 0.3:   # a click flag left set: the first pump keeps the button word (any value)
                put32(lib, img, "g_rootInput", rng.choice([1, 2, 3, 5]), 0x10)
                put32(lib, img, "g_rootInput", 1, 0x1C)
            from port.test import cmdemu as CE
            inputs, cursors = [], []
            if rng.random() < 0.3:   # a first batch that leaves the button word alone (a key-down)
                inputs, cursors = [CE.Raw(8, 0, 0)], [(rng.randrange(800), rng.randrange(0x102, 600))]
            for _ in range(rng.choice([1, 2, 4])):
                i, c = _picker_actions(rng, _zones(lib, img))
                inputs, cursors = inputs + i, cursors + c
            out.append((bytes(img), [rng.choice([0, 1])], inputs, cursors, rule, None))
        else:
            img = _editor_image(lib, rng, 0, 0, False)
            pts = _spots(lib, img) if name == "nearestCharControl" else _zones(lib, img)
            x, y = rng.choice(pts)
            j = rng.choice([0, 3, 0x14, 0x27, 0x28, 0x3B, 0x3C, 0x3D, 80, 400])
            x, y = x + rng.randrange(-j, j + 1), y + rng.randrange(-j, j + 1)
            if rng.random() < 0.3:
                d = rng.choice([0x27, 0x28, 0x29, 0x3B, 0x3C, 0x3D])
                x, y = rng.choice([(x + d, y), (x, y + d), (x - d, y), (x, y - d), (x + 3 * d, y)])
            if name == "charEditorZone" and rng.random() < 0.15:   # on the arrows' radius (0x3b / 0x3c from the centre)
                cx, cy = rng.choice([(0x30A, 0x16B), (0x12, 0x1EF)])
                d = rng.choice([0x3B, 0x3C])
                x, y = rng.choice([(cx + d, cy), (cx - d, cy), (cx, cy + d), (cx, cy - d)])
            out.append((bytes(img), [x, y], [], [], rule, None))
    return out


FUNCS = ["showCharacterEditor", "pickCharEditor", "nearestCharControl", "charEditorZone"]
ORACLE_N = {"showCharacterEditor": 300, "pickCharEditor": 150, "nearestCharControl": 400, "charEditorZone": 400}


@need_exe
@emu_oracle
@pytest.mark.parametrize("name", FUNCS)
def test_chareditor_equals_emulated_original(portlib, name):
    bad = []
    for k, case in enumerate(cases(portlib, name, ORACLE_N[name], 1)):
        d = compare(portlib, name, case)
        if d:
            bad.append((k, d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


# ------------------------------------------------------------------------------------------------ golden digests
GOLDEN_N = {"showCharacterEditor": 120, "pickCharEditor": 80, "nearestCharControl": 200, "charEditorZone": 200}
_CACHE = {}


def _golden_cases(lib, name):
    key = ("golden", name)
    if key not in _CACHE:
        _CACHE[key] = cases(lib, name, GOLDEN_N[name], "golden")
    return _CACHE[key]


def _port_digest(lib, name, case) -> str:
    import hashlib
    img, args, inputs, cursors, rule, salt = case
    pe, pimg, code, used, reads, calls = port_run(lib, name, img, args, inputs, cursors, rule, salt)
    tail = json.dumps([code, used, reads, [list(c) for c in calls]])
    return digest(pe, pimg) + ":" + hashlib.sha256(tail.encode()).hexdigest()[:12]


def _golden_port(lib, name):
    return [_port_digest(lib, name, c) for c in _golden_cases(lib, name)]


def _golden_load():
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


@need_exe
@pytest.mark.parametrize("name", FUNCS)
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
NEGCTL_RED = [("showCharacterEditor", m) for m in range(1, 10)] + [("pickCharEditor", 1), ("pickCharEditor", 2),
                                                                     ("nearestCharControl", 1), ("charEditorZone", 1)]


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


def test_every_chareditor_function_has_a_negctl(portlib):
    mine = {n for n, f in portlib.functions().items() if f.get("module") == "chareditor" and not n.startswith("chareditor.")}
    assert mine == {n for n, _ in NEGCTL_RED}, sorted(mine)


@need_exe
def test_editor_waits_without_an_input_source(portlib):
    """the editor reads the cursor in every pass: with no InputSource it stops (NeedInput = SG_E_NO_RESOURCE) at its
    first cursor read; the entry writes (the preview golfer, the backup record) are made before it, as in the original"""
    rng = random.Random(3)
    img = _editor_image(portlib, rng, 5, 7, True)
    with portlib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(bytes(img))
        with pytest.raises(L.SgError) as e:
            st.call("showCharacterEditor", [5, 7])
        assert e.value.code == L.SG_E_NO_RESOURCE
