"""Phase 3.8 stream 38g: showShortcutsScreen (module `routing`, port/src/sim/routing.cpp): the command effect of the keyboard
shortcuts help page, split from its drawing as 3.7b did. Evidence as in test_routing.py: the emulator oracle (opt-in
SG_EMU_ORACLE=1), golden digests (port/test/golden_shortcuts.json, pinned with SG_EMU_ORACLE=update) and negative controls.
"""
from __future__ import annotations

import gc
import json
import os
import pathlib
import random
import struct

import pytest

from port.test import lib as L
from port.test import test_commands as TC

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_shortcuts.json"
pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

NAME = "showShortcutsScreen"
ENTRY = 0x0044E770
# the drawing callees: all in the 3.7b screens' list (Image / Surface constructors and lifecycle, loadImage, blits, strings,
# the colour setup, window refresh, the modal-stack push)
DROPPED = TC.SCREEN_DROPPED + [0x00473AE0]   # + Surface_free (the list holds the other drawing callees)
MASK = TC.SCREEN_MASK
BOX = (0x2C6, 0x2F2, 0x16A, 0x196)


def _image(lib, rng):
    img = bytearray(TC._panel_image(lib, rng))
    TC.put(lib, img, "g_ui_569498", bytes([rng.choice([0, 1, 2, 9])]))
    struct.pack_into("<I", img, 0x00519CD8 - 0x004C1000, 0)
    struct.pack_into("<I", img, 0x0083AD50 - 0x004C1000, TC.FAKE_DISPLAY)
    if rng.random() < 0.8:   # a quiet input state: the loop waits for what the script delivers
        for nm in ("g_rootInput", "gDirtyFlagA", "gDirtyFlagB", "g_redrawPending", "g_flag_83afcc"):
            TC.put(lib, img, nm, bytes(TC.objs(lib)[nm][1]))
    return bytes(img)


def _inputs(rng):
    from port.test import cmdemu as CE
    out = []
    for _ in range(rng.choice([1, 1, 2, 3, 4])):
        k = rng.random()
        x, y = rng.randrange(800), rng.randrange(600)
        if k < 0.4:
            out += CE.click(x, y, rng.getrandbits(16), rng.getrandbits(16))
        elif k < 0.55:
            out += [CE.Raw(4, x, y, rng.getrandbits(16), rng.getrandbits(16)), CE.Raw(3, 0, 0)]
        elif k < 0.75:
            out += CE.key(rng.choice([0x1B, 0x41, 0x50, 0x0D]), rng.choice([0, 0x61, 0x1B]))
        else:
            out.append(CE.Raw(rng.choice([2, 3, 9]), x, y))
    return out


def _cursor(rng):
    x0, x1, y0, y1 = BOX
    if rng.random() < 0.7:   # on or around the close button's edges
        return rng.choice([x0 - 1, x0, x0 + 5, x1 - 17, x1 - 1, x1, rng.randrange(x0 - 4, x1 + 4)]), \
            rng.choice([y0 - 1, y0, y0 + 9, y1 - 1, y1, rng.randrange(y0 - 4, y1 + 4)])
    return rng.randrange(-10, 810), rng.randrange(-10, 610)


def iter_cases(lib, n, seed):
    rng = random.Random(f"{NAME}/{seed}")
    for _ in range(n):
        yield (_image(lib, rng), [], _inputs(rng), _cursor(rng))


def cases(lib, n, seed):
    return list(iter_cases(lib, n, seed))


def emu(img, args, inputs, cur):
    from port.test import cmdemu as CE
    e = CE.Emu()
    for a in DROPPED:
        if a in (0x00483D30, 0x00483C30, 0x00483C70, 0x00483BD0):   # tail-jumping helpers without arguments
            e.stub(a, 0, 0, False)
        else:
            e.drop(a)
    for a, n in TC.EH_VEC.items():
        e.stub(a, n, 0, True)
    e.stub(TC.VIEW_TO_LOCAL, 2, ("cursor", cur), True)
    e.stub(TC.FAKE_DISPLAY + 0x800, 0, 0, False, "display slot")
    vt = struct.pack("<I", TC.FAKE_DISPLAY + 0x100) + bytes(0xFC) + struct.pack("<I", TC.FAKE_DISPLAY + 0x800) * 0x100
    r = e.run(ENTRY, args, img, inputs=inputs, blocks=[(TC.FAKE_DISPLAY, vt.ljust(0x900, b"\xC3"))])
    gc.collect()
    return r


def port(lib, img, args, inputs, cur):
    import ctypes as C
    with lib.state() as st:
        st.set_data_base(TC.DATA_VA)
        st.load_image(img)
        words = [w for c in inputs for w in c.args()]
        if inputs:
            buf = (C.c_uint32 * len(words))(*words)
            st.map(TC.INPUT_VA, buf, readonly=True)
        st.call("commands.setInput", [TC.INPUT_VA, len(inputs)])
        st.call("commands.setCursor", [cur[0] & 0xFFFFFFFF, cur[1] & 0xFFFFFFFF])
        try:
            eax, code = st.call(NAME, list(args)).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        used = st.call("commands.inputConsumed", []).eax
        st.call("commands.clearInput", [])
        return eax, st.image(), code, used


def compare(lib, case) -> str:
    img, args, inputs, cur = case
    r = emu(img, args, inputs, cur)
    pe, pimg, code, used = port(lib, img, args, inputs, cur)
    if r.err.startswith("NeedInput"):
        return "" if code == L.SG_E_NO_RESOURCE else f"original waits for input, port {code} {pe}"
    if r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    if pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    if used != r.consumed:
        return f"input consumed port {used} original {r.consumed}"   # (eax is the stubbed Image destructor's: not compared)
    b = bytearray(pimg), bytearray(r.image)
    for im in b:
        for nm, at, ln in MASK:
            if nm in TC.objs(lib):
                off, size = TC.objs(lib)[nm]
                ln2 = size - at if ln is None else ln
                im[off + at:off + at + ln2] = bytes(ln2)
    return TC.first_diff(bytes(b[0]), bytes(b[1]), lib)


@need_exe
@emu_oracle
def test_shortcuts_equals_emulated_original(portlib):
    bad = []
    for k, case in enumerate(iter_cases(portlib, int(os.environ.get("SG_SHORTCUTS_N", "300")), 1)):
        d = compare(portlib, case)
        if d:
            bad.append((k, len(case[2]), case[3], d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


GOLDEN_N = 120
NEGCTL_RED = [
    (1, "a right click on the close button ends it too"),
    (2, "the close button's box is 0x10 columns narrower"),
    (3, "the exit leaves the message-box mode byte at 1"),
    (4, "a key does not end the screen"),
]


def _golden_cases(lib):
    key = ("golden", NAME)
    if key not in TC._BASE:
        TC._BASE[key] = cases(lib, GOLDEN_N, "golden")
    return TC._BASE[key]


def _golden_port(lib):
    out = []
    for img, args, inputs, cur in _golden_cases(lib):
        pe, pimg, code, used = port(lib, img, args, inputs, cur)
        out.append(TC.digest(0, pimg) + f":{code}:{used}")   # eax not part of the digest (see compare)
    return out


def _golden_load():
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


@need_exe
def test_golden_digests(portlib):
    if os.environ.get("SG_EMU_ORACLE") == "update":
        bad = [k for k, c in enumerate(_golden_cases(portlib)) if compare(portlib, c)]
        assert not bad, f"oracle RED on golden cases {bad[:5]}: not pinned"
        g = _golden_load()
        g[NAME] = _golden_port(portlib)
        GOLDEN.write_text(json.dumps(g, indent=0, sort_keys=True))
    g = _golden_load()
    if NAME not in g:
        pytest.skip(f"{NAME}: no pinned digests (SG_EMU_ORACLE=update)")
    got = _golden_port(portlib)
    bad = [k for k, (a, b) in enumerate(zip(got, g[NAME])) if a != b]
    assert not bad and len(got) == len(g[NAME]), f"{len(bad)} golden cases differ: {bad[:6]}"


@need_exe
@pytest.mark.parametrize("mode,what", NEGCTL_RED)
def test_negctl_reads_red(portlib, mode, what):
    g = _golden_load()
    if NAME not in g:
        pytest.skip(f"{NAME}: no pinned digests")
    portlib.negctl(NAME, mode)
    try:
        got = _golden_port(portlib)
        if os.environ.get("SG_EMU_ORACLE") == "1":
            red = sum(bool(compare(portlib, c)) for c in _golden_cases(portlib))
            assert red > 0, f"{NAME} mode {mode} ({what}) GREEN against the original"
    finally:
        portlib.negctl_reset()
    assert got != g[NAME], f"{NAME} mode {mode} ({what}) is blind on the golden cases"
