"""Roadmap S3.19: the character editor's bio EditBox in the graphics backend (port/src/sim/gfx_editbox.cpp, family `editbox`).

The bio EditBox is a window-tree child of the root with its own window surface. EditBox::drawContents (EditBox::on_redraw, the
EditBox vtable's slot +0x58) paints it: Window::paint, the bevel passes, the inset clip, wrapToLines, the caret's line, the
first visible line kept around the caret, drawTextLines and the caret line; Window::refreshFull composes it over the root's
copy. No checkpoint can show it (the bio loop reads no clock and the editor's next pass covers it: log/roadmap/S3/18_notes.md
section 5), so the evidence is

  * the CALL ORACLE (SG_EMU_ORACLE=1): the ORIGINAL drawContents under Unicorn (port/test/cmdemu.py) with recording callees
    (the handle's getBounds / callBounds, Window::paint, Widget_drawBevelBoxR, Widget_fillRectR, wrapToLines with a scripted
    line table, drawTextLines, drawMarkupRun with a scripted width rule, Widget_drawVLine) and lineIndexAt / Widget_value /
    Widget_get real, against the port's record mode on the same varied EditBox states: the same calls with the same arguments
    and the same first visible line at the end; the default suite pins the port's logs;
  * a COMPOSED FRAME of a constructed bio state (a segment start image, the host GDI text provider, the user's install): the
    box's margin is the paint colour, its text is inside the inset rectangle, the caret is a full-height black column at the
    text's end, everything outside the box is the root's copy, the GameState is unchanged, and EditBox.close removes it;
  * a BOOT run of cov_r_bio with the backend: the bio entries are drawn and no unsupported reason is left for them.
"""
from __future__ import annotations

import ctypes
import hashlib
import os
import random
import struct

import numpy as np
import pytest

from port.test import boot as B
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

NAME_VA = 0x30000000
BLK = 0x31000000             # text +0, fake FontRef +0x1000, script +0x2000, handle / vtable / stubs +0x3000 (the oracle)
TEXT_VA, FONT_VA, SCRIPT_VA = BLK, BLK + 0x1000, BLK + 0x2000
HANDLE_VA, VT_VA, STUB_GB, STUB_CB = BLK + 0x3000, BLK + 0x3100, BLK + 0x3800, BLK + 0x3810
DRAW_CONTENTS = 0x004864F0
ORACLE = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                            reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn")
BOX = (0x1E0, 0x70, 0x12C, 0xC8)
WHITE = 0x80007FFF


@pytest.fixture(scope="module")
def elib():
    lib = L.load()
    if not lib.has("gfx.family.editbox"):
        pytest.skip("the library has no editbox family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _off(lib, name):
    for n, off, _size in lib.objects():
        if n == name:
            return off
    raise KeyError(name)


def _g(st, name, args=()):
    raw = name.encode() + b"\0"
    ctypes.memmove(ctypes.addressof(st._namebuf), raw, len(raw))
    return st.call("gfx.call", [NAME_VA] + [a & 0xFFFFFFFF for a in args] + [0] * (11 - len(args))).eax


class _Negctl:
    def __init__(self, lib, mode):
        self.lib, self.mode = lib, mode

    def __enter__(self):
        self.prev = self.lib.negctl("gfx.family.editbox", self.mode)

    def __exit__(self, *e):
        self.lib.negctl("gfx.family.editbox", self.prev or 0)


# ------------------------------------------------------------------------------------------------ the call oracle's cases
def _visible(case):
    """the drawContents rule (the C2 transcription), only to keep a case inside the line table: the first line at the end"""
    r = [0, 0, case["w"], case["h"]]
    for _ in range(max(case["border"], 0)):
        r = [r[0] + 1, r[1] + 1, r[2] - 1, r[3] - 1]
    i = case["inset"]
    r = [r[0] + i, r[1] + i, r[2] - i, r[3] - i]
    offs, count = case["offs"], len(case["offs"]) - 1
    target = case["caret"]
    ci = 0
    while target >= offs[ci]:
        ci += 1
        if ci >= count:
            break
    ci -= 1
    f8, fc, f10 = case["font"]
    v = f10 + f8 if f8 >= 0 else fc
    diff = v - f10
    span = r[3] - r[1] + diff
    n = int(span / v)
    first, lim = case["first"], case["first"] + n - 1
    if ci < first:
        first = ci
    if ci > lim:
        first = ci - n + 1
    return ci, first


def _cases(n, seed):
    rnd = random.Random(seed)
    out = []
    while len(out) < n:
        ln = rnd.randrange(0, 90)
        text = bytes(rnd.randrange(0x20, 0x7F) for _ in range(ln))
        count = rnd.randrange(1, min(32, ln + 1) + 1) if ln else 1
        mids = sorted(rnd.sample(range(1, ln + 1), count - 1)) if ln and count > 1 else []
        offs = [0] + mids + [ln]
        f8 = rnd.choice([-1, -1, 0, 2, 5])
        case = dict(text=text, offs=offs, caret=rnd.randrange(0, ln + 1), first=rnd.randrange(0, count),
                    border=rnd.choice([0, 0, 0, 1, 2, 3]), inset=rnd.randrange(0, 8), w=rnd.randrange(40, 420),
                    h=rnd.randrange(20, 320), font=(f8, rnd.randrange(8, 32), rnd.randrange(6, 24)),
                    caret_on=rnd.choice([0, 1, 1]), paint=rnd.randrange(0, 256), light=rnd.randrange(0, 256),
                    dark=rnd.randrange(0, 256), colour=rnd.randrange(0, 256), run=(rnd.randrange(1, 6), rnd.choice([7, 15])))
        f8, fc, f10 = case["font"]
        if (f10 + f8 if f8 >= 0 else fc) == 0:
            continue
        ci, first = _visible(case)
        if not (0 <= first <= count) or ci < 0:
            continue   # the original reads its stack array outside the table there (the port reports it)
        out.append(case)
    return out


def _image(lib, case):
    img = bytearray(lib.state_size)
    eb = _off(lib, "g_characterEditorEditBox")
    p32 = lambda o, v: struct.pack_into("<I", img, eb + o, v & 0xFFFFFFFF)  # noqa: E731
    p32(0x278, HANDLE_VA)                 # the Image's handle (+0x274 + 4)
    p32(0x274 + 0x5C, FONT_VA)            # the Image's base font
    p32(0x274 + 0x6C, case["colour"])     # style 0's main colour: the caret's
    p32(0x574, TEXT_VA)
    p32(0x57C, case["first"])
    p32(0x584, case["paint"])
    p32(0x590, case["light"])
    p32(0x594, case["dark"])
    p32(0x598, case["border"])
    p32(0x59C, case["inset"])
    p32(0x5A4, case["caret"])
    p32(0x5A8, case["caret_on"])
    return bytes(img), eb


def _block(case):
    b = bytearray(0x4000)
    b[0:len(case["text"])] = case["text"]
    f8, fc, f10 = case["font"]
    struct.pack_into("<Iiii", b, 0x1000 + 4, 0x1234, f8, fc, f10)   # FontRef +4 (a font object), +8, +0xc, +0x10
    struct.pack_into("<iiIii", b, 0x2000, case["w"], case["h"], len(case["offs"]) - 1, case["run"][0], case["run"][1])
    for k, o in enumerate(case["offs"]):
        struct.pack_into("<I", b, 0x2014 + 4 * k, o)
    struct.pack_into("<I", b, 0x3000, VT_VA)
    struct.pack_into("<I", b, 0x3100 + 0xD0, STUB_GB)
    struct.pack_into("<I", b, 0x3100 + 0xC4, STUB_CB)
    b[0x3800:0x3820] = b"\xC3" * 0x20
    return b


def _port_calls(lib, case):
    img, eb = _image(lib, case)
    with lib.state() as st:
        st.set_data_base(SG.DATA_VA)
        st.load_image(img)
        st._namebuf = st.map(NAME_VA, bytearray(0x1000))
        blk = (ctypes.c_ubyte * 0x4000).from_buffer_copy(_block(case))
        st.map(BLK, blk)
        st.call("gfx.attach", [])
        _g(st, "editbox.record", [1])
        _g(st, "editbox.script", [SCRIPT_VA, SG.DATA_VA + eb])
        _g(st, "EditBox::drawContents", [SG.DATA_VA + eb, 0])
        _g(st, "editbox.record", [0])
        out = (st.files_read("$gfx/editbox_calls.txt") or b"").decode().splitlines()
        assert st.image() == img, "record mode wrote the GameState"
        st.call("gfx.detach", [])
    return out


def _emu_calls(emu, lib, case):
    img, eb = _image(lib, case)
    calls = []
    count = len(case["offs"]) - 1
    u = lambda v: str(v & 0xFFFFFFFF)  # noqa: E731

    def rect(uc, p):
        return " ".join(u(v) for v in struct.unpack("<iiii", bytes(uc.mem_read(p, 16))))

    def add(addr, nargs, fn, pops=None):
        emu.stub_py(addr, nargs, fn, emu.ret_pop(addr) > 0 if pops is None else pops)

    def getbounds(uc, a, ecx):
        uc.mem_write(a[0], struct.pack("<iiii", 0, 0, case["w"], case["h"]))
        calls.append("bounds")
        return 0

    def wrap(uc, a, ecx):
        calls.append("wrap")
        uc.mem_write(a[0], b"".join(struct.pack("<I", TEXT_VA + o) for o in case["offs"]))
        uc.mem_write(a[1], struct.pack("<I", count))
        return count

    def run(uc, a, ecx):
        calls.append(f"run {u(a[0] - TEXT_VA)} {u(a[1])}")
        n = a[1]
        data = bytes(uc.mem_read(a[0], n)) if 0 < n < 0x1000 else b""
        return sum(case["run"][0] + (c & case["run"][1]) for c in data)

    emu.stubs.clear()
    add(STUB_GB, 1, getbounds, True)
    add(STUB_CB, 0, lambda uc, a, ecx: calls.append("callBounds") or 0, False)
    add(0x004808C0, 1, lambda uc, a, ecx: calls.append(f"paint {u(a[0])}") or 0)
    add(0x004795D0, 3, lambda uc, a, ecx: calls.append(f"bevel {rect(uc, a[0])} {u(a[1])} {u(a[2])}") or 0)
    add(0x00475B00, 1, lambda uc, a, ecx: calls.append(f"clip {rect(uc, a[0])}") or 0)
    add(0x004862B0, 2, wrap)
    add(0x004782D0, 4, lambda uc, a, ecx: calls.append(f"lines {u(a[0] - TEXT_VA)} {rect(uc, a[1])} {u(a[2])} {u(a[3])}") or 0)
    add(0x00477280, 2, run)
    add(0x00478BE0, 4, lambda uc, a, ecx: calls.append("vline " + " ".join(u(v) for v in a[0:4])) or 0)
    r = emu.run(DRAW_CONTENTS, [], img, ecx=SG.DATA_VA + eb, blocks=[(BLK, bytes(_block(case)))])
    if r.err:
        # 32 lines: wrapToLines' end entry is the dword above drawContents' 32-entry line table, its return address, so the
        # original returns to the text's end (`ret` into TEXT_VA + the last offset): the port logs that as `fault return`
        if count >= 32 and f"eip {TEXT_VA + case['offs'][-1]:#x}" in r.err:
            return calls + ["fault return"]
        return calls + [f"error {r.err}"]
    calls.append("first " + u(struct.unpack_from("<i", r.image, eb + 0x57C)[0]))
    return calls


@ORACLE
def test_drawcontents_calls_equal_the_original(elib):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    cases = _cases(300, 0x5319)
    bad, branches = [], set()
    for i, case in enumerate(cases):
        want = _emu_calls(emu, elib, case)
        got = _port_calls(elib, case)
        moved = _visible(case)[1] - case["first"]
        branches.add((case["border"] > 0, case["caret_on"], any(c.startswith("first") for c in want),
                      (moved > 0) - (moved < 0)))
        if got != want:
            k = next((j for j in range(min(len(got), len(want))) if got[j] != want[j]), min(len(got), len(want)))
            bad.append((i, k, want[k:k + 2], got[k:k + 2]))
    assert not bad, f"{len(bad)} of {len(cases)} cases differ; first {bad[:3]}"
    # the cases reach both sides of every test of drawContents: bevel passes or not, the caret on / off, and the first visible
    # line LOWERED to a caret line above it (the store after `jge`), RAISED for a caret line below first + n - 1 (the store after
    # `jle`) and kept (127 / 45 / 128 of the 300)
    assert {b[0] for b in branches} == {False, True} and {b[1] for b in branches} == {0, 1}
    assert {b[3] for b in branches} == {-1, 0, 1}


CALL_NEGCTL = [(1, "the caret one pixel right"), (2, "the first line not raised below the window"),
               (3, "the inset left out of the text rectangle"), (4, "the visible-line count without value - get"),
               (8, "the bevel's colours swapped")]


@ORACLE
@pytest.mark.parametrize("mode,what", CALL_NEGCTL)
def test_drawcontents_calls_negctl_red(elib, mode, what):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    cases = _cases(120, 0x5319)   # control 4 first changes a call at case 80 (seven of the 300)
    differ = 0
    with _Negctl(elib, mode):
        for case in cases:
            if _port_calls(elib, case) != _emu_calls(emu, elib, case):
                differ += 1
    assert differ > 0, (mode, what)


# Default suite: the port's call logs of the oracle's first 120 cases, pinned (sha256 over the logs joined by a NUL) after the
# emulator oracle above was GREEN on them (SG_EMU_ORACLE=update prints a new digest instead of checking); every control above
# must change it.
CALLS_GOLDEN = "2252f06864aa3268bf47988f95c9fad23fd89ca0982b0d4e9b69ab25fba27ef6"


def _calls_digest(lib, n=120):
    h = hashlib.sha256()
    for case in _cases(n, 0x5319):
        h.update("\n".join(_port_calls(lib, case)).encode() + b"\0")
    return h.hexdigest()


def test_bodies_registered_without_duplicates(elib):
    with elib.state() as st:
        st.call("gfx.attach", [])
        assert st.call("gfx.check", []).eax == 0, st.files_read("$gfx/bodies.txt")
        st.call("gfx.detach", [])


def test_drawcontents_calls_golden(elib):
    d = _calls_digest(elib)
    if os.environ.get("SG_EMU_ORACLE") == "update":
        print(f"CALLS_GOLDEN = {d!r}")
        return
    assert d == CALLS_GOLDEN, d


@pytest.mark.parametrize("mode,what", CALL_NEGCTL)
def test_drawcontents_calls_golden_negctl_red(elib, mode, what):
    with _Negctl(elib, mode):
        d = _calls_digest(elib)
    assert d != CALLS_GOLDEN, (mode, what)


# ------------------------------------------------------------------------------------------------ the composed frame
BIO_TEXT = b"Florence likes afternoon naps, chocolate, and fast cars. Her favorite color is blue.Hello GXol!"


def _start_image():
    for scen in ("mood5_fixture", "sandbox_basic", "course_season"):
        d = SG.ROOT / "port" / "corpus" / "segments" / scen
        if d.is_dir():
            for p in sorted(d.iterdir()):
                if (p / "start.data.bin").exists():
                    return (p / "start.data.bin").read_bytes()
    return None


def _composed(lib, text=BIO_TEXT, caret=None, close=False, mode=0):
    """(back buffer, root, GameState before, GameState after) for the bio EditBox drawn and composed once"""
    img = _start_image()
    if img is None or not SG.INSTALL.is_dir() or not lib.has("gditext.attach"):
        pytest.skip("no segment start image, install or host text provider")
    eb = _off(lib, "g_characterEditorEditBox")
    img = bytearray(img)
    p32 = lambda o, v: struct.pack_into("<I", img, eb + o, v & 0xFFFFFFFF)  # noqa: E731
    p32(0x574, TEXT_VA)
    p32(0x57C, 0)
    p32(0x598, 0)
    p32(0x59C, 4)
    p32(0x5A4, len(text) if caret is None else caret)
    p32(0x5A8, 1)
    img = bytes(img)
    with lib.state() as st, _Negctl(lib, mode):
        st.set_data_base(SG.DATA_VA)
        st.load_image(img)
        st.files_root(SG.INSTALL)
        st._namebuf = st.map(NAME_VA, bytearray(0x1000))
        blk = (ctypes.c_ubyte * 0x4000).from_buffer_copy(text + bytes(0x4000 - len(text)))
        st.map(BLK, blk)
        st.call("gditext.attach", [])
        st.call("gfx.attach", [])
        root_win = SG.DATA_VA + _off(lib, "g_rootWindow")
        ebva = SG.DATA_VA + eb
        _g(st, "EditBox.window", [ebva, *BOX])
        _g(st, "EditBox::drawContents", [ebva, WHITE])
        _g(st, "Window::refreshFull", [root_win, 0])
        if close:
            _g(st, "EditBox.close", [ebva])
            _g(st, "Window::refreshFull", [root_win, 0])
        assert st.call("gfx.dump", [0]).eax == 0 and st.call("gfx.dump", [1]).eax == 0
        back = np.frombuffer(st.files_read("$gfx/back.555"), dtype="<u2").reshape(600, 800).copy()
        root = np.frombuffer(st.files_read("$gfx/root.555"), dtype="<u2").reshape(600, 800).copy()
        st.call("gfx.log", [])
        log = st.files_read("$gfx/log.txt").decode()
        after = st.image()
        st.call("gfx.detach", [])
        st.call("gditext.detach", [])
    return back, root, img, after, log


def _box(a):
    x, y, w, h = BOX
    return a[y:y + h, x:x + w]


def _frame_ok(back, root):
    """the structural properties of the composed bio box (WHITE background, inset 4, black text and caret)"""
    x, y, w, h = BOX
    out = back.copy()
    out[y:y + h, x:x + w] = root[y:y + h, x:x + w]
    if not (out == root).all():
        return "outside the box differs from the root"
    b = _box(back)
    m = np.ones_like(b, dtype=bool)
    m[4:h - 4, 4:w - 4] = False
    if not (b[m] == 0x7FFF).all():
        return "the margin is not the paint colour"
    inner = b[4:h - 4, 4:w - 4]
    if int((inner != 0x7FFF).sum()) < 200:
        return "no text in the box"
    # the caret: a black column of at least 21 rows (its line height of 25 from y1 = 25 line - 1, clipped to the inset 4 on
    # the first line); the glyphs' ClearType pixels never make one
    z = (inner == 0).astype(np.int32)
    best = np.zeros(z.shape[1], dtype=np.int32)
    run = np.zeros(z.shape[1], dtype=np.int32)
    for row in z:
        run = (run + 1) * row
        best = np.maximum(best, run)
    if int(best.max()) < 21:
        return "no caret column"
    return None


def test_composed_bio_frame(elib):
    back, root, before, after, log = _composed(elib)
    assert _frame_ok(back, root) is None, _frame_ok(back, root)
    assert before == after, "drawing the EditBox wrote the GameState"
    firsts = [ln for ln in log.splitlines() if ln.startswith("first\t")]
    assert not any("EditBox" in ln for ln in firsts), firsts


def test_composed_bio_frame_close(elib):
    back, root, *_ = _composed(elib, close=True)
    assert (back == root).all()


FRAME_NEGCTL = [(5, "the window composed one pixel lower"), (6, "Window::paint does not clear the surface")]


@pytest.mark.parametrize("mode,what", FRAME_NEGCTL)
def test_composed_bio_frame_negctl_red(elib, mode, what):
    back, root, *_ = _composed(elib, mode=mode)
    assert _frame_ok(back, root) is not None, (mode, what)


def test_composed_bio_close_negctl_red(elib):
    back, root, *_ = _composed(elib, close=True, mode=7)
    assert not (back == root).all()


def test_wrap_width_negctl_red(elib):
    text = (b"one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen "
            b"seventeen eighteen nineteen twenty twentyone twentytwo twentythree")
    a = _box(_composed(elib, text=text)[0])
    b = _box(_composed(elib, text=text, mode=9)[0])
    assert (a != b).any()


def test_caret_follows_the_caret_position(elib):
    a = _box(_composed(elib, caret=0)[0])
    b = _box(_composed(elib, caret=5)[0])
    assert (a != b).any()


# ------------------------------------------------------------------------------------------------ the boot run
def test_cov_r_bio_boot_draws_the_bio_box(elib):
    if not (B.TRACES / "cov_r_bio.trace").exists():
        pytest.skip("no cov_r_bio trace")
    seen = {}

    def before(st):
        st.call("gfx.attach", [])

    def after(st, _res):
        st.call("gfx.log", [])
        seen["log"] = st.files_read("$gfx/log.txt").decode()
        st.call("gfx.detach", [])

    res = B.run("cov_r_bio", elib, before=before, after=after)
    assert res.status == "PASS", res.detail
    log = seen["log"]
    handled = {ln.split("\t")[1]: int(ln.split("\t")[2]) for ln in log.splitlines() if ln.startswith("handled\t")}
    assert handled.get("EditBox::drawContents", 0) >= 2 and handled.get("EditBox.window", 0) == 2, handled
    assert handled.get("EditBox.close", 0) == 2
    firsts = [ln for ln in log.splitlines() if ln.startswith("first\t")]
    assert not any("bio" in ln or "EditBox" in ln for ln in firsts), firsts
