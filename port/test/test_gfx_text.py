"""Roadmap S3.7: the graphics backend's `text` family (port/src/sim/gfx_text.cpp): the exe's marked-up text on the root surface,
the message window drawWindow (the ticker), drawWindowFrame and the HUD panels' hover tips. Notes and the gate:
log/roadmap/S3/7_notes.md.

ORACLES
  text calls   (opt-in SG_EMU_ORACLE=1) the ORIGINAL drawText4049d0 / drawTextSurface / drawCenteredText / drawCenteredText2 /
               drawMessageBuffer / Text_put under the emulator (port/test/cmdemu.py), run down to the jgld surface: a fake surface
               whose vtable slots record setFont / the colour calls / the text draws / end, fake fonts with a scripted width
               rule, a fake run palette and display; the port's record mode (gfx_text.test op 1) logs the same calls from its
               engine. Cases: random texts with every markup delimiter, doubled braces, link and dropdown tokens, link targets,
               tabs, underscores, palette and direct colours, style fonts, both line heights of drawMessageBuffer
  window calls (opt-in) the ORIGINAL drawWindow with its drawing callees recorded (drawFrameR3 / drawFrameS2 / blitPanel /
               Surface_stretchBlit / Surface_drawScaled / composeGolferSprite, the display's setScale) and drawMessageBuffer real
               down to the surface, against the port's drawWindow.frame in record mode
  golden       the default suite pins sha256 digests of the port's logs of the oracle's cases (pinned after the oracle was
               GREEN; SG_EMU_ORACLE=update prints new ones); every negative control changes them
  frames       the pinned references (log/c3/alt/refs) through port/test/framegate.py: the ticker window's and the tips' OPAQUE
               pixels (two runs with the test-only root fill SG_GFX_HUD_BGFILL 0x0000 / 0x7fff, --no-live) equal the original
"""
from __future__ import annotations

import ctypes
import hashlib
import os
import pathlib
import random
import struct
import subprocess
import sys

import numpy as np
import pytest

from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

ROOT = pathlib.Path(__file__).resolve().parents[2]
NAME_VA = 0x30000000
BLK = 0x31000000          # the fake jgld objects (surface, display, palette, fonts) and their stub code
BLK_SIZE = 0x10000
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn")

# the original functions (golf_clean.exe)
F_DRAW_TEXT = 0x004049D0        # drawText4049d0(text, x, y, colour), cdecl
F_TEXT_SURFACE = 0x00404AD0     # drawTextSurface
F_CENTRED = 0x00404B70          # drawCenteredText
F_CENTRED2 = 0x00404BC0         # drawCenteredText2
F_MSGBUF = 0x0045B0D0           # drawMessageBuffer(cols, x, y, colour)
F_TEXT_PUT = 0x00477250         # Text_put(this, text), thiscall
F_DRAW_WINDOW = 0x0040D320      # drawWindow(x, y, p3, mode)
F_MALLOC, F_FREE = 0x004A504F, 0x004A5007
F_HOTLIST_ADD, F_DROPDOWN_SPOT = 0x004929B0, 0x00478A90

# layout of the fake block
S_OBJ, S_VT = BLK + 0x000, BLK + 0x100
FMT = BLK + 0x200
D_OBJ, D_VT = BLK + 0x300, BLK + 0x400
P_HOLD, P_OBJ, P_VT, P_ENT = BLK + 0x500, BLK + 0x600, BLK + 0x700, BLK + 0x800
F_OBJ = (BLK + 0x1100, BLK + 0x1200, BLK + 0x1300, BLK + 0x1400)   # font objects: (addr >> 8) & 7 = 1..4
F_VT = BLK + 0x1500
STUB = BLK + 0x2000
HEAP = BLK + 0x4000


@pytest.fixture(scope="module")
def tlib():
    lib = L.load()
    if not lib.has("gfx.family.text"):
        pytest.skip("the library has no text family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _off(lib, name):
    for n, off, _size in lib.objects():
        if n == name:
            return off
    raise KeyError(name)


def _gcall(st, name, args=()):
    buf = getattr(st, "_text_namebuf", None)
    if buf is None:
        buf = st._text_namebuf = st.map(NAME_VA, bytearray(0x1000))
    raw = name.encode() + b"\0"
    ctypes.memmove(ctypes.addressof(buf), raw, len(raw))
    return st.call("gfx.call", [NAME_VA] + [a & 0xFFFFFFFF for a in args] + [0] * (11 - len(args))).eax


# ------------------------------------------------------------------------------------------------ the cases
class Case:
    def __init__(self, op, args, text, fonts, fh, img):
        self.op, self.args, self.text, self.fonts, self.fh, self.img = op, args, text, fonts, fh, img


def _tokens(lib):
    """the three `$` tokens the scanners compare (f_strpool_4e4218 + 0x3c / + 0x30 / + 0x24), read from the user's exe"""
    img = _base_image(lib)
    o = _off(lib, "f_strpool_4e4218")
    def cs(k):
        b = img[o + k:o + k + 16]
        return b[:b.index(0)]
    return cs(0x3C), cs(0x30), cs(0x24)


_BASE = {}


def _base_image(lib) -> bytes:
    """a segment start image (the .data image of a continuous run: the user's exe constants and string pools included)"""
    if "img" not in _BASE:
        d = SG.SEGMENTS if hasattr(SG, "SEGMENTS") else ROOT / "port" / "corpus" / "segments"
        p = next((q / "start.data.bin" for sc in ("sandbox_basic", "championship", "course_season") if (d / sc).is_dir()
                  for q in sorted((d / sc).iterdir()) if (q / "start.data.bin").exists()), None)
        if p is None:
            pytest.skip("no segment start image")
        _BASE["img"] = p.read_bytes()
    return _BASE["img"]


def _rand_text(rnd, toks, n_words):
    link, drop, droplink = toks
    words = ["golf", "the", "Gary", "tee", "green", "x", "par_3", "rough", "Bunker", "{{brace", "end]]"]
    out = []
    for _ in range(n_words):
        r = rnd.random()
        if r < 0.45:
            out.append(rnd.choice(words))
        elif r < 0.55:
            out.append("{" + rnd.choice(words) + "}")
        elif r < 0.62:
            out.append("[" + rnd.choice(words) + "]")
        elif r < 0.66:
            out.append("\t" + rnd.choice(words))
        elif r < 0.70:
            out.append(rnd.choice(words) + "{")        # an unclosed style: it persists in the object
        elif r < 0.74:
            out.append("}" + rnd.choice(words))
        elif r < 0.78:
            out.append((link + b"=12>").decode("latin-1") + rnd.choice(words))
        elif r < 0.81:
            out.append((link + b"=abc>").decode("latin-1") + rnd.choice(words) + "}")
        elif r < 0.84:
            out.append("$x" + rnd.choice(words))
        elif r < 0.87:
            out.append(rnd.choice(words) + "^" + rnd.choice(words))
        elif r < 0.90:
            out.append(rnd.choice(words) + "\n" + rnd.choice(words))
        elif r < 0.93:
            out.append("=" + rnd.choice(words))
        else:
            out.append(rnd.choice(words) + "_" + rnd.choice(words))
    return " ".join(out)


def _cases(lib, n, seed):
    rnd = random.Random(seed)
    toks = _tokens(lib)
    base = _base_image(lib)
    o = {k: _off(lib, k) for k in ("g_rootWindow", "f_stateptr_4c1570", "g_font51b360", "g_fontRef_519fd8", "g_font519948",
                                   "g_font519928", "g_textBuffer", "g_engine83ad0c", "g_engine83ad18", "g_primarySurface",
                                   "g_richTextPairs")}
    disp_off = 0x0083AD50 - SG.DATA_VA
    fontrefs = [o["g_font51b360"], o["g_fontRef_519fd8"], o["g_font519948"], o["g_font519928"]]
    out = []
    for k in range(n):
        img = bytearray(base)
        p32 = lambda off, v: struct.pack_into("<I", img, off, v & 0xFFFFFFFF)  # noqa: E731
        root = SG.DATA_VA + o["g_rootWindow"] + 0x274
        ri = o["g_rootWindow"] + 0x274
        p32(o["f_stateptr_4c1570"], root)
        # the root Image as Image_ctor leaves it (c3dl.cpp ImageCtor_re), its surface the fake one
        for off_ in range(0, 0xb0, 4):
            p32(ri + off_, 0)
        p32(ri + 4, S_OBJ)
        p32(ri + 0x2c, 0xFFFFFFFF)
        p32(ri + 0x40, 1)
        p32(ri + 0x44, 0xFFFFFFFF)
        for s_ in range(4):
            p32(ri + 0x6c + 4 * s_, 0xFFFFFFFF)
            p32(ri + 0x7c + 4 * s_, 0xFFFFFFFF)
            p32(ri + 0x8c + 4 * s_, 2)
            p32(ri + 0x9c + 4 * s_, 2)
        p32(ri + 0xb0, 0)
        # the fonts: FontRef +4 = a fake font object, +8 / +0xc / +0x10 the line metrics
        fh = []
        for i, fr in enumerate(fontrefs):
            p32(fr + 4, F_OBJ[i])
            h10 = rnd.choice([12, 13, 16, 17, 19])
            h8 = rnd.choice([2, 4, 5])
            p32(fr + 8, h8)
            p32(fr + 0xc, 14)
            p32(fr + 0x10, h10)
            fh.append((h8, h10))
        p32(o["g_primarySurface"], SG.DATA_VA + fontrefs[0])
        p32(o["g_engine83ad18"], 0)
        p32(o["g_engine83ad0c"], P_HOLD)
        p32(disp_off, D_OBJ)
        for i in range(20):
            p32(o["g_richTextPairs"] + 4 * i, 0)
        f0 = SG.DATA_VA + fontrefs[rnd.choice([0, 1])]
        fonts = [f0, 0, 0, 0]
        if rnd.random() < 0.4:
            fonts[1] = SG.DATA_VA + fontrefs[2]
        if rnd.random() < 0.3:
            fonts[2] = SG.DATA_VA + fontrefs[3]
        if rnd.random() < 0.2:
            fonts[3] = SG.DATA_VA + fontrefs[1]
        for s_ in range(4):
            p32(ri + 0x5c + 4 * s_, fonts[s_])
        op = rnd.choice([3, 3, 4, 5, 6, 6, 7, 7, 7, 8])
        text = _rand_text(rnd, toks, rnd.randint(1, 9 if op != 7 else 30)).encode("latin-1")
        colour = rnd.choice([0x80007FFF, 0x80000018, 0x80006318, 0x80004210, rnd.randint(0, 0x7FFFFFFF) | 0x80000000, 1, 0,
                             rnd.randint(2, 200)])
        x, y = rnd.randint(-20, 700), rnd.randint(-20, 590)
        tb = o["g_textBuffer"]
        tva = SG.DATA_VA + tb + (0 if op == 7 else 0x800)
        img[tb:tb + 0x1000] = bytes(0x1000)
        img[tva - SG.DATA_VA:tva - SG.DATA_VA + len(text) + 1] = text + b"\0"
        if op == 7:
            cols = rnd.choice([0x10, 0x25, 0x2c, 0x63])
            colour = rnd.choice([0x80007FFF, 0x80007FFF, 0x80000018])
            args = [cols, x, rnd.randint(0, 0x240), colour]
        elif op == 8:
            args = [tva]
        else:
            args = [tva, x, y, colour]
        out.append(Case(op, args, text, fonts, fh, bytes(img)))
    return out


# ------------------------------------------------------------------------------------------------ the emulator side
OP_ENTRY = {3: F_DRAW_TEXT, 4: F_TEXT_SURFACE, 5: F_CENTRED, 6: F_CENTRED2, 7: F_MSGBUF}


def _block():
    b = bytearray(BLK_SIZE)
    def w(addr, v):
        struct.pack_into("<I", b, addr - BLK, v & 0xFFFFFFFF)
    w(S_OBJ, S_VT)
    w(D_OBJ, D_VT)
    w(P_HOLD + 4, P_OBJ)
    w(P_OBJ, P_VT)
    w(FMT, 0x10)
    for f in F_OBJ:
        w(f, F_VT)
    slots = {}
    def slot(vt, off, idx):
        w(vt + off, STUB + 0x10 * idx)
        slots[(vt, off)] = STUB + 0x10 * idx
    for i, off in enumerate((0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xE4, 0xE8, 0xD8, 0xDC)):
        slot(S_VT, off, i)
    slot(D_VT, 0xB4, 10)
    slot(D_VT, 0x9C, 11)
    slot(D_VT, 0xA0, 12)
    slot(P_VT, 0x20, 13)
    slot(F_VT, 0x10, 14)
    return bytes(b), slots


def _emu_calls(emu, case, extra=None):
    blk, slots = _block()
    log = []

    def cstr(uc, p, n=None):
        s = bytearray()
        while (n is None or len(s) < n) and len(s) < 0x400:
            ch = bytes(uc.mem_read(p + len(s), 1))
            if n is None and ch == b"\0":
                break
            s += ch
        return s.decode("latin-1")

    def st(addr, nargs, fn, pops=True):
        emu.stub_py(addr, nargs, fn, pops)

    st(slots[(S_VT, 0xA8)], 1, lambda uc, a, c: log.append(f"setFont {a[0]}") or 0)
    st(slots[(S_VT, 0xAC)], 0, lambda uc, a, c: log.append("end") or 0)
    st(slots[(S_VT, 0xB0)], 1, lambda uc, a, c: log.append(f"index {a[0]}") or 0)
    st(slots[(S_VT, 0xB4)], 3, lambda uc, a, c: log.append(f"rgb {a[0]} {a[1]} {a[2]}") or 0)
    st(slots[(S_VT, 0xB8)], 4, lambda uc, a, c: log.append(
        f"draw {struct.unpack('<i', struct.pack('<I', a[0]))[0]} {struct.unpack('<i', struct.pack('<I', a[1]))[0]} "
        f"{cstr(uc, a[2], a[3])}".replace(chr(10), chr(92) + "n")) or 0)
    st(slots[(S_VT, 0xE4)], 0, lambda uc, a, c: FMT)
    st(slots[(S_VT, 0xE8)], 0, lambda uc, a, c: 0)
    st(slots[(S_VT, 0xD8)], 0, lambda uc, a, c: 800)     # the surface's width / height (drawTextSurface's clamp)
    st(slots[(S_VT, 0xDC)], 0, lambda uc, a, c: 600)
    st(slots[(D_VT, 0xB4)], 0, lambda uc, a, c: 0)
    st(slots[(D_VT, 0x9C)], 3, lambda uc, a, c: log.append(f"Display::setScale {a[0]} {a[1]} {a[2]}") or 0)

    def get_scale(uc, a, c):
        for p in a:
            uc.mem_write(p, struct.pack("<i", 1))
        return 0
    st(slots[(D_VT, 0xA0)], 3, get_scale)

    def pal(uc, a, c):
        i = a[0] & 0xFF
        uc.mem_write(P_ENT + 4 * i, bytes([(3 * i) & 0xFF, (5 * i) & 0xFF, (7 * i) & 0xFF]))
        return P_ENT + 4 * i
    st(slots[(P_VT, 0x20)], 1, pal)

    def meas(uc, a, c):
        s = bytes(uc.mem_read(a[0], a[1])) if a[1] > 0 else b""
        return sum((ch & 0xF) + 3 for ch in s) + ((c >> 8) & 7) * len(s)
    st(slots[(F_VT, 0x10)], 2, meas)
    emu.stub(F_MALLOC, 1, HEAP, False, "malloc")
    emu.stub(F_FREE, 1, 0, False, "free")
    emu.stub(F_HOTLIST_ADD, 7, 0, True, "HotList::add")
    emu.stub(F_DROPDOWN_SPOT, 6, 0, True, "addDropdownSpot")
    if extra:
        extra(emu, log, cstr)
    root = 0x00519CD4   # the root Image (g_rootWindow + 0x274)
    if case.op == 8:
        r = emu.run(F_TEXT_PUT, case.args, case.img, ecx=root, blocks=[(BLK, blk)])
        log.append(f"width {struct.unpack('<i', struct.pack('<I', r.eax or 0))[0]}")
    elif case.op == 9:
        r = emu.run(F_DRAW_WINDOW, case.args, case.img, blocks=[(BLK, blk)])
    else:
        r = emu.run(OP_ENTRY[case.op], case.args, case.img, blocks=[(BLK, blk)])
    return log, r


# ------------------------------------------------------------------------------------------------ the port side
def _port_calls(lib, case):
    with lib.state() as st:
        st.set_data_base(SG.DATA_VA)
        st.load_image(case.img)
        st.call("gfx.attach", [])
        root = SG.DATA_VA + _off(lib, "g_rootWindow") + 0x274
        _gcall(st, "setDrawTarget", [root] + list(case.fonts))
        _gcall(st, "gfx_text.test", [1])
        if case.op == 9:
            _gcall(st, "gfx_text.test", [9] + list(case.args))
        else:
            _gcall(st, "gfx_text.test", [case.op] + list(case.args))
        _gcall(st, "gfx_text.test", [2])
        raw = (st.files_read("$gfx/text_calls.txt") or b"").decode("latin-1")
        st.call("gfx.detach", [])
    return [ln for ln in raw.split("\n") if ln]


def _first_diff(a, b):
    k = next((j for j in range(min(len(a), len(b))) if a[j] != b[j]), min(len(a), len(b)))
    return k, a[k:k + 2], b[k:k + 2]


N_CASES = 400
SEED = 0x7E47


@emu_oracle
def test_text_calls_equal_the_original(tlib):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    compared, bad, faults = 0, [], []
    for i, case in enumerate(_cases(tlib, N_CASES, SEED)):
        want, r = _emu_calls(emu, case)
        if r.err:
            faults.append((i, r.err))
            continue
        got = _port_calls(tlib, case)
        compared += 1
        if got != want:
            bad.append((i, case.op, case.text[:60], _first_diff(want, got)))
    print(f"text calls: {compared} cases compared, {len(faults)} faulted in the original")
    assert compared >= 350, f"only {compared} cases ran in the original; faults {faults[:3]}"
    assert not bad, f"{len(bad)} of {compared} cases differ; first {bad[:3]}"


# ------------------------------------------------------------------------------------------------ drawWindow
F_WRAP = 0x0045AF30            # wrapMessageBuffer(cols): stubbed, the case's text is wrapped already (the port's layout wraps)
WINDOW_STUBS = {               # address: (name, nargs, thiscall, how the line is built from (args, ecx))
    0x0040CEF0: ("drawFrameR3", 5, False, lambda a, c: a[:5]),
    0x0040D0B0: ("drawFrameS2", 5, False, lambda a, c: a[:5]),
    0x0040CA10: ("blitPanel", 5, False, lambda a, c: a[:5]),
    0x00473F60: ("Surface_stretchBlit", 5, True, lambda a, c: [c] + a[:5]),
    0x00473CB0: ("Surface_drawScaled", 7, True, lambda a, c: [c] + a[:7]),
    0x00473E60: ("Image_draw", 4, True, lambda a, c: [c] + a[:4]),
    0x00478AF0: ("Win_fillRect", 5, True, lambda a, c: [c] + a[:5]),
    0x00462020: ("composeGolferSprite", 1, False, lambda a, c: a[:1]),
}
# playSound (mode 0's slide-in sound) is the layout's (commands.cpp draw_window_effect), not drawing: stubbed, not compared


def _window_cases(lib, n, seed):
    rnd = random.Random(seed)
    base_cases = _cases(lib, n, seed ^ 0x55)
    o = {k: _off(lib, k) for k in ("f_w_4c2e00", "g_value587ea0", "g_textBuffer", "g_lastAllocatedGolfer")}
    out = []
    for k, c in enumerate(base_cases):
        img = bytearray(c.img)
        sel = rnd.choice([-1, -1, -1, rnd.randint(0, 40), -2, -3, -4, -5, -6, -9, -0xe, -0x14, -0x15, -0x16, -0x10])
        mode = rnd.choice([-3, -2, -1, 0, 1, 2, 3]) if sel == -1 else rnd.choice([-3, -2, -1, 2])
        words = _rand_text(rnd, _tokens(lib), rnd.randint(2, 30)).replace("^", " ")
        r = rnd.randint(0, 6)
        lines = words.split(" ")
        text = ""
        for i, w in enumerate(lines):   # a pre-wrapped text: r newlines among the words
            text += w + ("\n" if i < r else " ")
        text = text.encode("latin-1")[:0x300]
        h = 15 * r + 0x20
        H = 0x60 if sel != -1 and h < 0x60 else h
        struct.pack_into("<i", img, o["f_w_4c2e00"] + 8, sel)
        struct.pack_into("<i", img, o["g_value587ea0"], H)
        tb = o["g_textBuffer"]
        img[tb:tb + 0x1000] = bytes(0x1000)
        img[tb:tb + len(text) + 1] = text + b"\0"
        x, y = rnd.choice([(0xF0, -8), (0xC8, 0x154), (0xC8, 0xC8), (0x96, rnd.randint(0, 0x180)),
                           (rnd.randint(-40, 600), rnd.randint(-40, 400))])
        case = Case(9, [x, y, mode, h], text, c.fonts, c.fh, bytes(img))
        case.r, case.sel = r, sel
        out.append(case)
    return out


def _emu_window(emu, case):
    def extra(e, log, cstr):
        for addr, (name, nargs, this, pick) in WINDOW_STUBS.items():
            def fn(uc, a, ecx, name=name, pick=pick):
                log.append(name + "".join(" " + str(v & 0xFFFFFFFF) for v in pick(a, ecx)))
                return 0
            e.stub_py(addr, nargs, fn, this)
        e.stub(F_WRAP, 1, case.r, False, "wrapMessageBuffer")
        e.stub(0x004481B0, 5, 0, False, "playSound")
    c2 = Case(9, [case.args[0], case.args[1], 0x80007FFF, case.args[2]], case.text, case.fonts, case.fh, case.img)
    return _emu_calls(emu, c2, extra)


@emu_oracle
def test_window_calls_equal_the_original(tlib):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    compared, bad, faults = 0, [], []
    for i, case in enumerate(_window_cases(tlib, 300, SEED + 1)):
        want, r = _emu_window(emu, case)
        if r.err:
            faults.append((i, r.err))
            continue
        got = _port_calls(tlib, case)
        compared += 1
        if got != want:
            bad.append((i, case.sel, case.args, _first_diff(want, got)))
    print(f"window calls: {compared} cases compared, {len(faults)} faulted in the original")
    assert compared >= 270, f"only {compared} cases ran in the original; faults {faults[:3]}"
    assert not bad, f"{len(bad)} of {compared} cases differ; first {bad[:3]}"


# ------------------------------------------------------------------------------------------------ the hover tips
# kind: (the original function, the port's hit test, the gfx_text.test op, the tip's place in the original's call log)
TIP_KINDS = {
    "tool": (0x00433190, "hitTestSlot432f90", 10, "prefix"),
    "slot": (0x00433190, "hitTestSlot432f90", 11, "suffix"),
    "toolbar": (0x00433E50, "hitBuildTool433c60", 12, "suffix"),
    "action": (0x004362F0, "hitGolferPanel435f00", 13, "suffix"),
    "view": (0x00435760, "hitEmployeeSlot435570", 14, "suffix"),
    "detail": (0x00436E50, "hitEmployee436b00", 15, "suffix"),
    "amenities": (0x00434CF0, "pickAmenity434980", 16, "suffix"),
    "row": (0x00434CF0, None, 17, "backdrops"),
}
TIP_NAMES = ("drawPanelBackdrop", "drawMapLine", "drawCenteredText")
PANEL_BG = 0x00541A44   # g_sprites541a18 + 0x2c: showInfoCursorPanel's panel background, the first call after its tool tip
TIP_STUBS = {           # recorded drawing callees of the panels (address: name, nargs, thiscall, line args)
    0x0040CEF0: ("drawFrameR3", 5, False, lambda a, c: a[:5]),
    0x00473F60: ("Surface_stretchBlit", 5, True, lambda a, c: [c] + a[:5]),
    0x00473CB0: ("Surface_drawScaled", 7, True, lambda a, c: [c] + a[:7]),
    0x00473E60: ("Image_draw", 4, True, lambda a, c: [c] + a[:4]),
    0x00478B80: ("Widget_fillRect", 5, True, lambda a, c: [c] + a[:5]),
    0x004493D0: ("drawMapLine", 7, False, lambda a, c: a[:7]),
    0x00480B00: ("fillWindowRect", 6, True, lambda a, c: [c] + a[:6]),
    0x004628D0: ("addSprite", 8, False, lambda a, c: a[:8]),
    0x00407C60: ("drawCircle", 4, False, lambda a, c: a[:4]),
    0x004049D0: ("drawText4049d0", 4, False, lambda a, c: a[1:4]),
}


def _amenity_row_hit(p1, p2, p3, p7, p8, p10):
    """commands_layout.inc amenity_row_hit (the mode-4 row under the cursor), for the row tip's r"""
    if p2 == 0x1A3:
        p3 *= 3
    n47 = 47 * p3
    x0 = p1 - ((n47 + ((n47 >> 31) & 3)) >> 2) + 0x220
    res, a = -1, 0
    for i in range(p3):
        if p10 == 0 or p10 & (1 << (i & 31)):
            px = (a + (1 if a < 0 else 0)) // 2 + x0 + 0x2C if a >= 0 else int((a + 1) / 2) + x0 + 0x2C
            py = 0x1F4 + (0x1B if i & 1 else 0)
            dx, dy = abs(p7 - px), abs(2 * (p8 - py))
            d = dx + dy - min(dx, dy) // 2   # approxDistance (util.cpp)
            if d < 0x14:
                res = i
        a += 0x2F
    return res


def _tip_cases(lib, kind, n, seed):
    rnd = random.Random(seed)
    o = {k: _off(lib, k) for k in ("g_panelState", "g_ui_5aa554", "g_buildPanelSlot", "g_ui_561254", "f_w_4c2848",
                                   "g_amenityUnlockMask", "g_ui_59ca54", "g_zoomScale")}
    base = _cases(lib, n * 2, seed)
    entry, hit_fn, op, _ = TIP_KINDS[kind]
    out = []
    with lib.state() as st:
        for c in base:
            if len(out) >= n:
                break
            img = bytearray(c.img)
            p32 = lambda off, v: struct.pack_into("<i", img, off, v)  # noqa: E731
            p32(o["g_buildPanelSlot"], 1 if kind in ("amenities", "row") else 0)
            p32(o["g_ui_561254"], 0)
            p32(o["g_ui_59ca54"], 1)
            if kind == "row":
                p32(o["f_w_4c2848"] + 0xC, 4)
                p32(o["g_amenityUnlockMask"], rnd.randint(1, 0x3FFF))
            elif kind == "amenities":
                p32(o["f_w_4c2848"] + 0xC, rnd.choice([1, 2, 3, 0x10, 0x13, 0]))
            # the cursor: a random grid point of the panels' area whose hit is a tip's (the port's registered hit tests)
            def ok(h):
                if h == -1 or (kind == "tool" and h >= 0x14) or (kind == "slot" and h < 0x14):
                    return False
                return not (kind == "detail" and h in (2, 3))
            if kind == "row":
                mask = struct.unpack_from("<I", img, o["g_amenityUnlockMask"])[0]
                pts = [(a_, b_) for a_ in range(0xC0, 0x330, 5) for b_ in range(0x1C0, 0x258, 5)
                       if _amenity_row_hit(0, 0x168, 0xE, a_, b_, mask) != -1]
                if not pts:
                    continue
                a, b = rnd.choice(pts)
                hit = _amenity_row_hit(0, 0x168, 0xE, a, b, mask)
                p32(o["g_panelState"] + 4, -7)   # no dwell tip in the same call
                p32(o["g_ui_5aa554"], 0)
            else:
                st.set_data_base(SG.DATA_VA)
                st.load_image(bytes(img))
                hv = lambda a_, b_: struct.unpack("<i", struct.pack("<I", st.call(hit_fn, [a_, b_]).eax))[0]  # noqa: E731
                pts = [(a_, b_) for a_ in range(0xC0, 0x330, 6) for b_ in range(0x170, 0x258, 6) if ok(hv(a_, b_))]
                if not pts:
                    continue
                a, b = rnd.choice(pts)
                hit = hv(a, b)
                p32(o["g_panelState"] + 4, hit)
                p32(o["g_ui_5aa554"], 11)
            case = Case(op, [hit, a, b], c.text, c.fonts, c.fh, bytes(img))
            case.kind, case.entry = kind, entry
            out.append(case)
    return out


def _emu_tip(emu, case):
    def extra(e, log, cstr):
        for addr, (name, nargs, this, pick) in TIP_STUBS.items():
            if case.kind == "tool" and name in ("drawCenteredText",):
                continue
            def fn(uc, a, ecx, name=name, pick=pick):
                log.append(name + "".join(" " + str(v & 0xFFFFFFFF) for v in pick(a, ecx)))
                return 0
            e.stub_py(addr, nargs, fn, this)

        def backdrop(uc, a, ecx):
            log.append(f"drawPanelBackdrop {a[0] & 0xFFFFFFFF} {a[1] & 0xFFFFFFFF}|{cstr(uc, 0x0051A068)}")
            return 0
        e.stub_py(0x00432620, 2, backdrop, False)
        if case.kind != "tool":
            def centred(uc, a, ecx):
                log.append(f"drawCenteredText {a[1] & 0xFFFFFFFF} {a[2] & 0xFFFFFFFF} {a[3] & 0xFFFFFFFF}|{cstr(uc, a[0])}")
                return 0
            e.stub_py(0x00404B70, 4, centred, False)
        e.stub(0x0045B660, 2, 0, False, "lookupThemeText")   # file input: no theme text in both arms
        e.stub(0x0040AA80, 0, 0, False, "hireEmployee")
        e.stub(0x004481B0, 5, 0, False, "playSound")
    c2 = Case(99, [case.args[1], case.args[2]], case.text, case.fonts, case.fh, case.img)
    blk, _ = _block()
    log, r = _emu_calls_entry(emu, c2, case.entry, extra)
    return log, r


def _emu_calls_entry(emu, case, entry, extra):
    saved = OP_ENTRY.get(99)
    OP_ENTRY[99] = entry
    try:
        return _emu_calls(emu, case, extra)
    finally:
        if saved is None:
            OP_ENTRY.pop(99, None)


def _tip_part(kind, log):
    if kind == "tool":
        k = next((i for i, l in enumerate(log) if l.startswith(f"Surface_stretchBlit {PANEL_BG} ")), len(log))
        return log[:k]
    if kind == "row":
        return [l for l in log if l.startswith("drawPanelBackdrop")]
    k = len(log)
    while k > 0 and log[k - 1].split(" ", 1)[0].split("|", 1)[0] in TIP_NAMES:
        k -= 1
    return log[k:]


@emu_oracle
@pytest.mark.parametrize("kind", list(TIP_KINDS))
def test_tip_calls_equal_the_original(tlib, kind):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    cases = _tip_cases(tlib, kind, 60, SEED + 7 + list(TIP_KINDS).index(kind))
    compared, bad, faults = 0, [], []
    for i, case in enumerate(cases):
        log, r = _emu_tip(emu, case)
        if r.err:
            faults.append((i, r.err))
            continue
        want = _tip_part(kind, log)
        got = _port_calls(tlib, case)
        compared += 1
        if got != want:
            bad.append((i, case.args, _first_diff(want, got)))
    print(f"tip {kind}: {compared} cases equal-compared, {len(faults)} faulted, distinct hits {len({c.args[0] for c in cases})}")
    assert compared >= 20, f"{kind}: only {compared} of {len(cases)} cases ran in the original; faults {faults[:3]}"
    assert not bad, f"{kind}: {len(bad)} of {compared} cases differ; first {bad[:2]}"


# ------------------------------------------------------------------------------------------------ negative controls, goldens
# (mode, the oracle that reads it RED, what it breaks); mode 5 (Text_put's width to the frame code) has no call log: frames
NEGCTL = [(1, "text", "`{` does not change the style"), (2, "text", "a doubled `{{` draws no literal brace"),
          (3, "text", "the run drawer ignores tabs"), (4, "text", "the shadow run one pixel lower"),
          (6, "window", "a golfer / picture window one pixel right"), (7, "window", "drawWindowFrame's strip one pixel lower"),
          (8, "tool", "the tool tip's frame one pixel right"), (9, "text", "drawMessageBuffer breaks 8 pixels later"),
          (10, "toolbar", "the drawPanelBackdrop tips one pixel lower"), (11, "amenities", "the value line one pixel lower")]


def _oracle_cases(lib, which, n):
    if which == "text":
        return _cases(lib, n, SEED)
    if which == "window":
        return _window_cases(lib, n, SEED + 1)
    return _tip_cases(lib, which, n, SEED + 7 + list(TIP_KINDS).index(which))


def _differ(lib, which, n, mode):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    prev = lib.negctl("gfx.family.text", mode)
    try:
        d = 0
        for case in _oracle_cases(lib, which, n):
            if which == "text":
                want, r = _emu_calls(emu, case)
            elif which == "window":
                want, r = _emu_window(emu, case)
            else:
                log, r = _emu_tip(emu, case)
                want = _tip_part(which, log)
            if not r.err and _port_calls(lib, case) != want:
                d += 1
    finally:
        lib.negctl("gfx.family.text", prev or 0)
    return d


@emu_oracle
@pytest.mark.parametrize("mode,which,what", NEGCTL)
def test_oracle_negctl_red(tlib, mode, which, what):
    assert _differ(tlib, which, 120 if which in ("text", "window") else 40, mode) > 0, (mode, what)


# Default suite: sha256 of the port's record-mode logs over the oracles' cases (the first 120 text / window cases, 30 per tip
# kind), pinned after the emulator oracle above was GREEN on them (SG_EMU_ORACLE=update prints new digests); every control
# above changes them
GOLDEN_SETS = [("text", 120), ("window", 120)] + [(k, 30) for k in TIP_KINDS]
GOLDEN = {
    'text': 'ddd623d96bbc75bb9e3ac8c955550d06d23d2dc4e5342686678dec1968c0f41b',
    'window': 'c588af6fce1fbf74cc878a444371fda73a5f7c4e77383325a99236131f1bc0d3',
    'tool': 'd339867e18e8daa3ac088b1802056a52fcd33377da7a269cde807ed26ebbbc2e',
    'slot': 'ba6eb97e91e3b396ec8522fc89a3ed8f676cd669592073d4ff4be38414f0edfb',
    'toolbar': 'c4599575c766320da42438e608f9185489932667e26a364f9ae9af7a7401c40b',
    'action': '57970e1a7b366d10ece81c5b14122c7a28d3a0bf3602fb30b57f233dd426d85d',
    'view': '24088d355ac13dac97912137f8adb3f6d2eda87a45638cad99a43303aa7c5588',
    'detail': '82e1f092955d6c2e28b886688a35503a3942f6f6f6dbdf314c80ea218960ece4',
    'amenities': 'e9f923673656ba0b07d03e79d04aaa1e7900ec206aa2b6a54ebf2ab4d918af11',
    'row': '7a38ed6144f7048e7dbdd73266506d8e1fe406fc0427f1ad7ac11acec88663f5',
}


def _digest(lib, which, n):
    h = hashlib.sha256()
    for case in _oracle_cases(lib, which, n):
        h.update("\n".join(_port_calls(lib, case)).encode("latin-1") + b"\0")
    return h.hexdigest()


@pytest.mark.parametrize("which,n", GOLDEN_SETS)
def test_text_calls_golden(tlib, which, n):
    d = _digest(tlib, which, n)
    if os.environ.get("SG_EMU_ORACLE") == "update" or which not in GOLDEN:
        print(f"GOLDEN[{which!r}] = {d!r}")
        assert which in GOLDEN or os.environ.get("SG_EMU_ORACLE") == "update", f"no pinned digest for {which}"
        return
    assert d == GOLDEN[which], f"{which}: {d}"


@pytest.mark.parametrize("mode,which,what", NEGCTL)
def test_text_calls_golden_negctl_red(tlib, mode, which, what):
    n = dict(GOLDEN_SETS)[which]
    prev = tlib.negctl("gfx.family.text", mode)
    try:
        d = _digest(tlib, which, n)
    finally:
        tlib.negctl("gfx.family.text", prev or 0)
    assert d != GOLDEN[which], (mode, what)


# ------------------------------------------------------------------------------------------------ frames
# The text family's OPAQUE pixels against the original's back buffer (S3.6's two-fill measure, gfx_hud.cpp after_present's
# test-only SG_GFX_HUD_BGFILL, framegate --no-live): per checkpoint the rectangle the family's drawing covers there (measured as
# the pixels that changed between a run without the family and one with it, log/roadmap/S3/7_notes.md)
from PIL import Image  # noqa: E402

from port.test import gfx as GX  # noqa: E402

FRAMES = {   # scenario: {checkpoint: (x, y, w, h)}
    "championship": {"tour_0060": (0xE0, 0, 0x1C4, 0x8C)},                  # the ticker with Gary's portrait
    "cov_a_flags": {"zdemo": (0x238, 0x180, 0xA0, 0x70),                     # showInfoCursorPanel's tool tip, theme text
                    "end": (0x40, 0x60, 0x170, 0x34)},                       # a golfer's thought box placed by Text_put's width
    "cov_r_tutorial": {"p01": (0xE4, 0, 0x1F0, 0x1EE), "p10": (0xE4, 0, 0x1F0, 0x1EE),   # tutorial windows
                       "p15": (0xE4, 0, 0x1F0, 0x40)},                       # the ticker with no golfer (drawWindowFrame)
    "cov_k1_label": {"label": (0x104, 0x18, 0x1A0, 0xCF)},                   # a help window
}


def _fill_runs(scenario, chks, out, negctl=None):
    imgs = {}
    for tag, fill in (("f0", "0x0000"), ("f1", "0x7fff")):
        d = out / tag
        cmd = [sys.executable, "-m", "port.test.framegate", scenario, "--checkpoints", ",".join(chks), "--no-probe", "--no-live",
               "--images", str(d), "--out", str(out / f"{tag}.tsv")]
        if negctl:
            cmd += ["--negctl", negctl]
        subprocess.run(cmd, env=dict(os.environ, SG_GFX_HUD_BGFILL=fill), check=True, capture_output=True, text=True,
                       cwd=str(ROOT))
        imgs[tag] = {c: np.asarray(Image.open(d / f"{scenario}.{c}.port.png").convert("RGB")) for c in chks}
    return imgs


def _bad(imgs, scenario, chk):
    a0, a1 = imgs["f0"][chk], imgs["f1"][chk]
    ref = GX.reference(scenario, chk)
    x, y, w, h = FRAMES[scenario][chk]
    opaque = (a0 == a1).all(axis=2)[y:y + h, x:x + w]
    bad = opaque & (a0 != ref).any(axis=2)[y:y + h, x:x + w]
    return int(opaque.sum()), int(bad.sum())


@pytest.fixture(scope="module")
def frame_runs(tlib, tmp_path_factory):
    if not GX.REFS.is_dir() or not SG.INSTALL.is_dir() or not tlib.has("gditext.attach"):
        pytest.skip("no pinned references, install or host text provider")
    return {s: _fill_runs(s, list(c), tmp_path_factory.mktemp(s)) for s, c in FRAMES.items()}


@pytest.mark.parametrize("scenario,chk", [(s, c) for s, cs in FRAMES.items() for c in cs])
def test_text_frames_equal_the_original(frame_runs, scenario, chk):
    opaque, bad = _bad(frame_runs[scenario], scenario, chk)
    assert opaque > 2000 and bad == 0, (opaque, bad)


FRAME_NEGCTL = [("championship", "gfx.family.text=6", "the golfer window one pixel right"),
                ("cov_a_flags", "gfx.family.text=8", "the tool tip's frame one pixel right"),
                ("cov_r_tutorial", "gfx.family.text=9", "drawMessageBuffer breaks 8 pixels later"),
                ("cov_r_tutorial", "gfx.family.text=7", "drawWindowFrame's strip one pixel lower"),
                ("cov_a_flags", "gfx.family.text=5", "Text_put's width to the frame code 8 more")]
# Controls no pinned frame reads RED (log/roadmap/S3/7_notes.md section 4): 4 the shadow run (drawWindow's and the tips' message
# texts never take drawTextSurface's shadow: g_font51b360's line height is 15, g_fontRef_519fd8's 10); 1 / 2 / 3 / 10 / 11 were
# not run on the frames (no pinned frame of this step's shows that markup or that tip); all six read RED on the oracles above.
FRAME_NEGCTL_BLIND = {4: "no shadowed message text in a pinned frame", 1: "not run on frames", 2: "not run on frames",
                      3: "not run on frames", 10: "not run on frames", 11: "not run on frames"}


@pytest.mark.parametrize("scenario,negctl,what", FRAME_NEGCTL)
def test_text_frames_negctl_red(frame_runs, tmp_path, scenario, negctl, what):
    chks = list(FRAMES[scenario])
    imgs = _fill_runs(scenario, chks, tmp_path, negctl)
    assert any(_bad(imgs, scenario, c)[1] > 0 for c in chks), (negctl, what)
