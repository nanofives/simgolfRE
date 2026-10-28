"""Roadmap S3.6: the graphics backend's `hud` family (port/src/sim/gfx_hud.cpp): the in-game Surface_blit3 cases, the whole image
half of loadInterfaceArt(1) and the HUD panels' drawing. Notes and the gate: log/roadmap/S3/6_notes.md.

ORACLES
  cut geometry   every SpriteRef the backend cuts during a boot run has the kept size and trims (cropSprite's +0x10, +0x14,
                 +0x20, +0x24) the ORIGINAL's record holds in a segment start image of the same scenario (the .data image of a
                 continuous run at a frame head, port/corpus/segments/<scenario>/<k>/start.data.bin)
  frames         the reference back buffers of the pinned runs (log/c3/alt/refs), through port/test/framegate.py, per HUD region
  unit           Surface_blit3's copy / magnification rules on constructed surfaces
  panel calls    (roadmap S3.6b, log/roadmap/S3/6b_notes.md) the employee panels drawEmployeeViewPanel (the golfer list) and
                 drawEmployeeDetailPanel: the ORIGINAL functions' drawing calls under the emulator oracle (SG_EMU_ORACLE=1)
                 against the port's call log on varied segment start images; the default suite checks pinned digests of
                 the port's logs and that the panel drawing leaves the GameState unchanged
"""
from __future__ import annotations

import ctypes
import os
import pathlib
import struct

import numpy as np
import pytest

from port.test import boot as B
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

ROOT = pathlib.Path(__file__).resolve().parents[2]
SEGS = ROOT / "port" / "corpus" / "segments"
NAME_VA = 0x30000000


@pytest.fixture(scope="module")
def hlib():
    lib = L.load()
    if not lib.has("gfx.family.hud"):
        pytest.skip("the library has no hud family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _gcall(st, name, args=()):
    buf = getattr(st, "_hud_namebuf", None)
    if buf is None:
        buf = st._hud_namebuf = st.map(NAME_VA, bytearray(0x1000))
    raw = name.encode() + b"\0"
    ctypes.memmove(ctypes.addressof(buf), raw, len(raw))
    return st.call("gfx.call", [NAME_VA] + [a & 0xFFFFFFFF for a in args] + [0] * (11 - len(args))).eax


def boot_sprites(lib, scenario: str, records: int = 2, negctl: tuple[str, int] | None = None) -> dict[int, tuple]:
    """The SpriteRefs the backend holds after `records` frame records of a boot run of `scenario` (key -> (w, h, left, top))."""
    out: dict[int, tuple] = {}

    def before(st):
        st.call("gfx.attach", [])

    def after(st, _res):
        _gcall(st, "gfx_hud.dumpSprites")
        raw = st.files_read("$gfx/hud_sprites.txt") or b""
        for ln in raw.decode().splitlines():
            k, w, h, l, t = (int(x) for x in ln.split())
            out[k] = (w, h, l, t)
        st.call("gfx.detach", [])

    prev = lib.negctl(negctl[0], negctl[1]) if negctl else None
    try:
        res = B.run(scenario, lib, max_records=records, before=before, after=after)
    finally:
        if negctl:
            lib.negctl(negctl[0], prev or 0)
    assert res.status in ("PASS", "KNOWN"), res.detail
    return out


def original_records(scenario: str, keys) -> dict[int, tuple]:
    """The original's (kept w, kept h, left, top) of each SpriteRef key, from the scenario's first segment start image."""
    seg = sorted(p for p in (SEGS / scenario).iterdir() if (p / "start.data.bin").exists())[0]
    img = (seg / "start.data.bin").read_bytes()
    out = {}
    for k in keys:
        off = k - SG.DATA_VA
        if 0 <= off and off + 0x28 <= len(img):
            w, h = struct.unpack_from("<ii", img, off + 0x10)
            l, t = struct.unpack_from("<ii", img, off + 0x20)
            out[k] = (w, h, l, t)
    return out


def geometry_mismatches(ours: dict, orig: dict) -> list:
    return [(hex(k), ours[k], orig[k]) for k in sorted(ours) if k in orig and ours[k] != orig[k]]


def boot_uncut(lib, scenario: str, records: int | None = None) -> dict[int, int]:
    """The SpriteRefs the HUD draws read during a boot run that no modelled cut reached (key -> draws)."""
    out: dict[int, int] = {}

    def before(st):
        st.call("gfx.attach", [])

    def after(st, _res):
        _gcall(st, "gfx_hud.dumpUncut")
        for ln in (st.files_read("$gfx/hud_uncut.txt") or b"").decode().splitlines():
            k, n = (int(x) for x in ln.split())
            out[k] = n
        st.call("gfx.detach", [])

    res = B.run(scenario, lib, max_records=records, before=before, after=after)
    assert res.status in ("PASS", "KNOWN"), res.detail
    return out


def original_handles(scenario: str, keys) -> dict[int, int]:
    """The original's handle word (+4) of each SpriteRef key in every segment start image of the scenario (the largest)."""
    out = {k: 0 for k in keys}
    for seg in sorted(p for p in (SEGS / scenario).iterdir() if (p / "start.data.bin").exists()):
        img = (seg / "start.data.bin").read_bytes()
        for k in keys:
            out[k] = max(out[k], struct.unpack_from("<I", img, k - SG.DATA_VA + 4)[0])
    return out


@pytest.mark.parametrize("scenario", ["cov_a_build"])
def test_uncut_spriterefs_have_no_handle_in_the_original(hlib, scenario):
    """A SpriteRef the HUD draws with no modelled cut must be one whose handle is NULL in the original (its draw is a no-op)."""
    uncut = boot_uncut(hlib, scenario)
    assert uncut, "the run drew no uncut SpriteRef (the check would be vacuous)"
    h = original_handles(scenario, uncut)
    bad = {hex(k): hex(v) for k, v in h.items() if v != 0}
    assert not bad, f"SpriteRefs with a handle in the original but no cut in the backend: {bad}"


@pytest.mark.parametrize("scenario", ["sandbox_basic", "cov_a_build"])
def test_cut_geometry_equals_the_original(hlib, scenario):
    ours = boot_sprites(hlib, scenario)
    orig = original_records(scenario, ours)
    bad = geometry_mismatches(ours, orig)
    assert len(orig) > 1000, f"only {len(orig)} SpriteRefs compared"
    assert not bad, f"{len(bad)} of {len(orig)} SpriteRefs differ, first {bad[:8]}"


# ------------------------------------------------------------------------------------------------ the employee panels (S3.6b)
def _obj(lib, name):
    for n, off, _size in lib.objects():
        if n == name:
            return off
    raise KeyError(name)


@pytest.mark.parametrize("scenario", ["cov_b_holes18", "cov_a_play"])
def test_employee_panels_leave_the_gamestate_unchanged(hlib, scenario):
    """Both panel bodies on a segment start image (the detail panel with an employee selected, so its text block runs
    appendWalkerName, appendDate and appendNumber, the golfer list with its buildGolferName per row) leave every GameState
    byte as they found it: the names go through g_textBuffer and the buffer is put back (gfx_hud.cpp buffer_text /
    scratch_text)."""
    segs = sorted(p for p in (SEGS / scenario).iterdir() if (p / "start.data.bin").exists()) if (SEGS / scenario).is_dir() else []
    if not segs:
        pytest.skip(f"no segment start image of {scenario}")
    img = bytearray((segs[-1] / "start.data.bin").read_bytes())
    emp = _obj(hlib, "g_walkers") + 0x10
    pick = next((k for k in range(64) if 0x80 <= img[emp + 0x4c * k + 3] != 0xfa and img[emp + 0x4c * k + 2] != 0), None)
    golfers = _obj(hlib, "g_golfers")
    listed = sum(1 for g in range(0x98) if 0 < img[golfers + 0x100 * g + 0x21] < 0x80)
    if pick is not None:   # slot 0 of this frame's list is the first employee when the scroll is 0
        struct.pack_into("<i", img, _obj(hlib, "g_ui_5aa558"), 0)
        struct.pack_into("<i", img, _obj(hlib, "g_employeePanelMode"), 0)
    with hlib.state() as st:
        st.set_data_base(SG.DATA_VA)
        st.load_image(bytes(img))
        st.call("gfx.attach", [])
        before = st.image()
        for name, xy in (("drawEmployeeDetailPanel.draw", (0x2c0, 0x220)), ("drawEmployeeViewPanel.draw", (0x150, 0x200))):
            _gcall(st, name, xy)
        after = st.image()
        st.call("gfx.log", [])
        log = (st.files_read("$gfx/log.txt") or b"").decode(errors="replace")
        st.call("gfx.detach", [])
    diff = [i for i in range(len(before)) if before[i] != after[i]][:8]
    assert not diff, f"GameState bytes changed by the panel drawing at offsets {[hex(i) for i in diff]}"
    assert "drawEmployeeViewPanel: the golfer list" not in log and "drawEmployeeDetailPanel: the detail panel" not in log
    assert pick is not None or listed, f"{scenario}: no employee and no golfer on the course (the check would be vacuous)"


# EMULATOR ORACLE of the two panels' drawing (opt-in SG_EMU_ORACLE=1, port/test/cmdemu.py): the ORIGINAL drawEmployeeViewPanel /
# drawEmployeeDetailPanel run under Unicorn over a GameState image with every drawing callee replaced by a recorder (its
# `ret N` convention), the hit tests, clamp, cellAt and the text builders (buildGolferName, pickGolferName, appendDate,
# appendNumber, __itoa) real, hireEmployee and playSound stubbed; the port's bodies log the same calls (gfx_hud.recordPanels).
# The two call sequences must be equal: callee, its object / image argument, position, size and colour, and the text of a
# text call. The pixel bodies those calls reach are judged elsewhere (the frames, the S2 / S3.6 families).
import random  # noqa: E402

emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn")
PANEL_ENTRY = {"view": 0x00435760, "detail": 0x00436E50}
ZOOM_VA = 0x004C2844   # g_zoomScale, read by the drawCircle recorder (the original sets it around the call)


def _emu_panel_calls(emu, img: bytes, which: str, x: int, y: int, cover=None):
    from unicorn import x86_const as X  # noqa: F401
    calls, skip_text = [], [False]

    def cstr(uc, p):
        s = bytearray()
        while len(s) < 0x400:
            ch = bytes(uc.mem_read(p + len(s), 1))
            if ch == b"\0":
                break
            s += ch
        return s.decode("latin-1")

    def add(addr, nargs, fn):
        pops = emu.ret_pop(addr) > 0
        emu.stub_py(addr, nargs, fn, pops)

    def rec(name, pick, text_arg=None):
        def fn(uc, a, ecx):
            if name == "drawCenteredText" and skip_text[0]:
                skip_text[0] = False
                return 0
            line = name + "".join(" " + str(v & 0xFFFFFFFF) for v in pick(a, ecx))
            if text_arg is not None:
                line += "|" + cstr(uc, a[text_arg])
            calls.append(line)
            return 0
        return fn

    add(0x00473F60, 5, rec("Surface_stretchBlit", lambda a, c: [c, a[0], a[2], a[3]]))
    add(0x00473E60, 4, rec("Image_draw", lambda a, c: [c, a[1], a[2]]))
    add(0x00473CB0, 7, rec("Surface_drawScaled", lambda a, c: [c, a[1], a[2]]))
    add(0x004762D0, 4, rec("setDrawTarget", lambda a, c: [a[0]]))
    add(0x004049D0, 4, rec("drawText4049d0", lambda a, c: a[1:4], text_arg=0))
    add(0x00404B70, 4, rec("drawCenteredText", lambda a, c: a[1:4], text_arg=0))
    add(0x00480B00, 5, rec("fillWindowRect", lambda a, c: a[0:5]))
    add(0x004628D0, 8, rec("addSprite", lambda a, c: a[0:8]))

    def circle(uc, a, ecx):
        z = struct.unpack("<i", bytes(uc.mem_read(ZOOM_VA, 4)))[0]
        calls.append("drawCircle" + "".join(" " + str(v & 0xFFFFFFFF) for v in a[0:4]) + " " + str(z & 0xFFFFFFFF))
        return 0
    add(0x00407C60, 4, circle)

    def tip(uc, a, ecx):
        calls.append("tooltip")
        skip_text[0] = which == "view"   # the view panel's tip text follows its drawMapLine bar
        return 0
    add(0x004493D0, 7, tip)
    add(0x00432620, 2, tip)
    emu.stub(0x0040AA80, 0, 0, False, "hireEmployee")
    emu.stub(0x004481B0, 5, 0, False, "playSound")
    r = emu.run(PANEL_ENTRY[which], [x & 0xFFFFFFFF, y & 0xFFFFFFFF], img, cover=cover)
    return calls, r


def _port_panel_calls(lib, img: bytes, which: str, x: int, y: int):
    name = {"view": "drawEmployeeViewPanel.draw", "detail": "drawEmployeeDetailPanel.draw"}[which]
    with lib.state() as st:
        st.set_data_base(SG.DATA_VA)
        st.load_image(img)
        st.call("gfx.attach", [])
        _gcall(st, "gfx_hud.recordPanels", [1])
        _gcall(st, name, [x, y])
        _gcall(st, "gfx_hud.recordPanels", [0])
        raw = (st.files_read("$gfx/hud_panel_calls.txt") or b"").decode("latin-1")
        st.call("gfx.detach", [])
    return [ln for ln in raw.splitlines() if ln]


def _panel_cases(lib, which: str, n: int, seed: int):
    """(image, x, y) cases from the segment start images of three scenarios, with the panels' inputs varied: the zoom (two
    layouts), the scroll, golfer and employee fields (status, partner, mood, colour words, flags, sheet and animation words,
    dates), the selection, the hire trigger, the cursor over every button and slot, and the dwell counter."""
    rnd = random.Random(seed)
    off = {n_: _obj(lib, n_) for n_ in ("g_golfers", "g_walkers", "g_zoomScale", "g_ui_5aa6b8", "g_ui_561254", "g_ui_5aa558",
                                         "g_employeePanelMode", "g_ui_59ca54", "g_panelState", "g_ui_5aa554", "f_w_4c2e00",
                                         "f_tab_point16_4c7af0", "g_lastAllocatedGolfer", "g_sheetCols", "g_cameraRot")}
    images = []
    for scen in ("cov_b_holes18", "cov_a_play", "ui_tour"):
        d = SEGS / scen
        if d.is_dir():
            images += [(p / "start.data.bin").read_bytes() for p in sorted(d.iterdir()) if (p / "start.data.bin").exists()]
    if not images:
        pytest.skip("no segment start images")
    pts = []
    for i in range(16):
        b = images[0]
        px_, py_ = struct.unpack_from("<hh", b, off["f_tab_point16_4c7af0"] + 4 * i)
        if px_ == -1:
            break
        pts.append((px_ + 8, py_ + 0x1e) if i in (2, 3) else (px_ + (0x10 if i < 8 else 0x20), py_ + 0x10))
    view_pts = [(0x11D, 0x1EC), (0x100, 0x1FE), (0xE8, 0x21A), (0x14C, 0x250), (0x304, 0x250), (0x150, 0x200), (0x2A0, 0x240),
                (0x50, 0x50)]
    detail_pts = [(0x11E, 0x1EC), (0x100, 0x1FE), (0x50, 0x50)] + pts
    out = []
    for k in range(n):
        img = bytearray(rnd.choice(images))
        p32 = lambda o, v: struct.pack_into("<i", img, o, v)  # noqa: E731
        g0 = off["g_golfers"]
        for _ in range(rnd.randrange(0, 40)):          # golfer fields the list reads
            g = rnd.randrange(0x98)
            r = g0 + 0x100 * g
            img[r + 0x21] = rnd.choice([0, 0, 1, 5, 9, 10, 0x13, 0xFF, 0xFE]) & 0xFF
            struct.pack_into("<H", img, r + 0xA2, rnd.choice([0xFFFF, rnd.randrange(0x98)]))
            for o in (0xA4, 0xA6, 0xA8, 0xAA):
                struct.pack_into("<h", img, r + o, rnd.choice([-3, 0, 5, 0x11, 0xA1, 8]))
            struct.pack_into("<h", img, r + 0xB0, rnd.choice([-1, 0, 2]))
            struct.pack_into("<h", img, r + 0xB2, rnd.choice([0, 1, 3]))
            if rnd.random() < 0.3:
                img[r + 0x13] ^= 0x20                     # bit 29 of dword +0x10
        shape = rnd.random()
        if shape < 0.15:                                  # nobody listed: no scroll bar
            for g in range(0x98):
                img[g0 + 0x100 * g + 0x21] = 0
                struct.pack_into("<H", img, g0 + 0x100 * g + 0xA2, 0xFFFF)
        elif shape < 0.4:                                 # a crowd: the compact layout runs past its last column
            for g in rnd.sample(range(0x98), 70):
                img[g0 + 0x100 * g + 0x21] = rnd.choice([1, 3, 12])
        if rnd.random() < 0.4:                            # pairs listed through the partner (the golfer itself not on course)
            for _ in range(6):
                a_, b_ = rnd.sample(range(0x98), 2)
                img[g0 + 0x100 * a_ + 0x21] = rnd.choice([0, 0xFF])
                img[g0 + 0x100 * b_ + 0x21] = rnd.choice([2, 11])
                struct.pack_into("<H", img, g0 + 0x100 * a_ + 0xA2, b_)
                struct.pack_into("<H", img, g0 + 0x100 * b_ + 0xA2, a_)
        p32(off["g_zoomScale"], rnd.choice([1, 2, 4]))
        p32(off["g_ui_5aa6b8"] + 0x10, rnd.choice([0, 0, 1, 7, 0x10, 0x40]))
        p32(off["g_ui_5aa6b8"] + 0x8, rnd.choice([-1, 3, 7, 9, 0x13]))
        p32(off["g_ui_5aa558"], rnd.choice([0, 0, 1, 2, 4]))
        p32(off["g_employeePanelMode"], rnd.choice([-1, -1, 0, 1, 2, 5, 7]))
        p32(off["g_ui_59ca54"], rnd.choice([0, 1, 1]))
        p32(off["g_cameraRot"], rnd.randrange(8))
        e0 = off["g_walkers"] + 0x10
        for _ in range(rnd.randrange(0, 24)):          # employee records the detail panel lists
            e = e0 + 0x4C * rnd.randrange(64)
            img[e + 2] = rnd.choice([0, 1, 8, 9])
            img[e + 3] = rnd.choice([0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0x00, 0x03]) & 0xFF
            img[e + 0] = rnd.randrange(0x40)
            img[e + 1] = rnd.randrange(0x40)
            img[e + 6] = rnd.randrange(8)
            struct.pack_into("<h", img, e + 0xC, rnd.randrange(-4, 40))
            struct.pack_into("<h", img, e + 0xE, rnd.choice([0, 0xA, 0xB, 0xC]))
            struct.pack_into("<h", img, e + 0x12, rnd.randrange(0, 60))
            struct.pack_into("<h", img, e + 0x14, rnd.randrange(-5, 300))
            struct.pack_into("<h", img, e + 0x16, rnd.randrange(-9, 99))
        if rnd.random() < 0.25:                           # a full staff (20 and more listed: the full image, the refusal)
            for k in range(64):
                e = e0 + 0x4C * k
                img[e + 2] = rnd.choice([1, 8, 9])
                img[e + 3] = rnd.choice([0xFE, 0xFD, 0xFC, 0xFB]) & 0xFF
                struct.pack_into("<h", img, e + 0xE, rnd.choice([0, 0xB, 0xC]))
        if rnd.random() < 0.3:                            # the selected employee a type -4 with flag 8 (the larger circle)
            sel = struct.unpack_from("<i", img, off["g_employeePanelMode"])[0]
            if sel >= 0:
                k = struct.unpack_from("<i", img, _obj(lib, "g_employeeDetailList") + 4 * sel)[0]
                if 0 <= k < 64:
                    img[e0 + 0x4C * k + 2] |= 8
                    img[e0 + 0x4C * k + 3] = 0xFC
        p32(off["f_w_4c2e00"] + 0x10, rnd.choice([-1, 0, 1, 2]))
        x, y = rnd.choice(view_pts if which == "view" else detail_pts)
        if rnd.random() < 0.6:                          # the dwell counter (past 10 or not) with a previous hover
            p32(off["g_panelState"] + 4, rnd.choice([-1, -2, -3, -4, 0, 1, 2, 4, 5, 8, 9]))
            p32(off["g_ui_5aa554"], rnd.choice([3, 11, 11]))
        p32(off["g_ui_561254"], 0)
        out.append((bytes(img), x, y))
    return out


@emu_oracle
@pytest.mark.parametrize("which", ["view", "detail"])
def test_panel_calls_equal_the_original(hlib, which):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    cases = _panel_cases(hlib, which, 240, 0x5E6B + (which == "detail"))
    compared, bad, faults = 0, [], 0
    for i, (img, x, y) in enumerate(cases):
        want, r = _emu_panel_calls(emu, img, which, x, y)
        if r.err:
            faults += 1     # the original faults (an unmapped read through a varied record, e.g. pickGolferName): not compared
            continue
        got = _port_panel_calls(hlib, img, which, x, y)
        compared += 1
        if got != want:
            k = next((j for j in range(min(len(got), len(want))) if got[j] != want[j]), min(len(got), len(want)))
            bad.append((i, k, want[k:k + 2], got[k:k + 2]))
    assert compared >= 200, f"only {compared} cases ran in the original ({faults} faulted)"
    assert not bad, f"{len(bad)} of {compared} cases differ; first {bad[:3]}"


PANEL_NEGCTL = [("view", 11, "the golfer list's names one pixel right"), ("view", 12, "the scroll bar 7 px tall"),
                ("detail", 13, "the list-slot images one pixel lower"), ("detail", 14, "the text block one pixel lower"),
                ("detail", 15, "no selection: the selection branch's button images")]


@emu_oracle
@pytest.mark.parametrize("which,mode,what", PANEL_NEGCTL)
def test_panel_calls_negctl_red(hlib, which, mode, what):
    from port.test import cmdemu as CE
    emu = CE.Emu()
    cases = _panel_cases(hlib, which, 40, 0x5E6B + (which == "detail"))
    prev = hlib.negctl("gfx.family.hud", mode)
    try:
        differ = 0
        for img, x, y in cases:
            want, r = _emu_panel_calls(emu, img, which, x, y)
            if not r.err and _port_panel_calls(hlib, img, which, x, y) != want:
                differ += 1
    finally:
        hlib.negctl("gfx.family.hud", prev or 0)
    assert differ > 0, (mode, what)


# Default suite: the port's call logs of the oracle's first 60 cases per panel, pinned (sha256 over the logs joined by a
# NUL) after the emulator oracle above was GREEN on them (SG_EMU_ORACLE=update prints new digests instead of checking);
# every control above must change them.
import hashlib  # noqa: E402

PANEL_GOLDEN = {"view": "a9500ce1fe5417919f85cf2f83a8023bee33019c1ff24a5f059da8021ec9719b",
                "detail": "3dc480129b613c8e1f1765a4c2928fc2d53f2910bd4d450bfe802e352abe967e"}


def _panel_digest(lib, which, n=60):
    h = hashlib.sha256()
    for img, x, y in _panel_cases(lib, which, n, 0x5E6B + (which == "detail")):
        h.update("\n".join(_port_panel_calls(lib, img, which, x, y)).encode("latin-1") + b"\0")
    return h.hexdigest()


@pytest.mark.parametrize("which", ["view", "detail"])
def test_panel_calls_golden(hlib, which):
    d = _panel_digest(hlib, which)
    if os.environ.get("SG_EMU_ORACLE") == "update":
        print(f"PANEL_GOLDEN[{which!r}] = {d!r}")
        return
    assert d == PANEL_GOLDEN[which], f"{which}: {d}"


@pytest.mark.parametrize("which,mode,what", PANEL_NEGCTL)
def test_panel_calls_golden_negctl_red(hlib, which, mode, what):
    prev = hlib.negctl("gfx.family.hud", mode)
    try:
        d = _panel_digest(hlib, which)
    finally:
        hlib.negctl("gfx.family.hud", prev or 0)
    assert d != PANEL_GOLDEN[which], (mode, what)


# ------------------------------------------------------------------------------------------------ controls on the cut oracles
@pytest.mark.parametrize("mode,what", [(3, "the art loads do nothing"), (4, "every cut inverts its trim")])
def test_cut_geometry_negctl_red(hlib, mode, what):
    # since S3 wave 2 the FLIC decoder (flic.cpp sheet_to_gfx) cuts every sheet cell into the backend as well (about 47,000
    # SpriteRefs of sandbox_basic), so a run whose interface-art loads do nothing still compares far more than 1,000 equal
    # records: the control reads RED when an interface-art SpriteRef of the plain run is missing or any record differs
    # (roadmap S3.6b)
    plain = boot_sprites(hlib, "sandbox_basic")
    ours = boot_sprites(hlib, "sandbox_basic", negctl=("gfx.family.hud", mode))
    orig = original_records("sandbox_basic", ours)
    assert set(plain) - set(ours) or geometry_mismatches(ours, orig), what


# ------------------------------------------------------------------------------------------------ frames
# The HUD regions' OPAQUE pixels against the original's back buffer. The runs draw no terrain (framegate --no-live: S3.2's
# terrain is judged under G2, not pixel-exact) and no scene sprites (S3.5), so two framegate runs fill the root after every present with 0x0000 and 0x7fff (test-only SG_GFX_HUD_BGFILL,
# gfx_hud.cpp after_present); a pixel equal in both was drawn opaquely in the snapshot's frame and must equal the reference.
# Checkpoints chosen where no other step's drawing reaches a HUD region (log/roadmap/S3/6_notes.md, the gate table).
import os
import subprocess
import sys

from PIL import Image

from port.test import framegate as FG
from port.test import gfx as GX

HUD = ("hud_title", "hud_meters", "toolbar", "bottom_panel")
FRAMES = {
    "cov_a_build": ["terrain_07", "amenities", "amen_470", "amen_717", "bldg_5", "undo"],
    "cov_a_golfer": ["action_0", "pan_rot0", "pan_rot2"],
    "cov_a_champ": ["gary_turn", "shot_keys", "champ_CJ"],
    "cov_b_shots": ["shot_08", "shot_16", "shot_24"],
    "ui_tour": ["people"],   # the employee detail panel with no selection and no employee (roadmap S3.6b)
}


def _fill_runs(scenario, chks, out, negctl=None):
    # --no-live: since S3.2 the terrain is drawn into the root (opaquely, within G2, not pixel-exact); the HUD measure keeps it
    # out as it was before S3.2 (roadmap S3.6b, log/roadmap/S3/6b_notes.md)
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
    bad = (a0 == a1).all(axis=2) & (a0 != ref).any(axis=2)
    regs = dict(FG.INGAME_REGIONS)
    out = {}
    for n in HUD:
        x, y, w, h = regs[n]
        out[n] = int(bad[y:y + h, x:x + w].sum())
    return out


@pytest.fixture(scope="module")
def frame_runs(hlib, tmp_path_factory):
    if not GX.REFS.is_dir() or not SG.INSTALL.is_dir() or not hlib.has("gditext.attach"):
        pytest.skip("no pinned references, install or host text provider")
    return {s: _fill_runs(s, c, tmp_path_factory.mktemp(s)) for s, c in FRAMES.items()}


@pytest.mark.parametrize("scenario,chk", [(s, c) for s, cs in FRAMES.items() for c in cs])
def test_hud_regions_equal_the_original(frame_runs, scenario, chk):
    assert _bad(frame_runs[scenario], scenario, chk) == dict.fromkeys(HUD, 0)


FRAME_NEGCTL = [("cov_a_build", "gfx.family.hud=6", "Surface_drawScaled one pixel right"),
                ("cov_a_champ", "gfx.family.hud=9", "shot button 4 with the next frame image"),
                ("cov_b_shots", "gfx.family.hud=10", "the scoreboard's hole columns one pixel right"),
                ("ui_tour", "gfx.family.hud=15", "the detail panel's buttons with no selection take the selection images")]
# Controls no pinned frame can read RED (log/roadmap/S3/6_notes.md section 4, U-1350 / U-1351 / U-1357), each measured:
#   1 a 16-bit copy one row short: no frame of cov_a_golfer / cov_b_shots / cov_a_keys changes (the copies the frames take
#     return the root's own content); 2 the magnification one source column right: 4 pixels of cov_b_shots end change, in
#     the zoom view over terrain the port does not draw (log/roadmap/S3/6_ctl12); 5 the half-bright table never chosen: the
#     shaded pixels are blends over the background, which the opaque measure leaves out; 8 a scaled drawB one source column
#     right: the scaled thought bubbles and heads are drawn between checkpoints, no judged frame changes (6_ctl8).
FRAME_NEGCTL_BLIND = {1: "16-bit copies equal their source", 2: "magnified pixels lie over undrawn terrain",
                      5: "shaded pixels are blends", 8: "no snapshot frame holds a scaled drawB"}


@pytest.mark.parametrize("scenario,negctl,what", FRAME_NEGCTL)
def test_hud_regions_negctl_red(frame_runs, tmp_path, scenario, negctl, what):
    chks = FRAMES[scenario]
    imgs = _fill_runs(scenario, chks, tmp_path, negctl)
    assert any(sum(_bad(imgs, scenario, c).values()) > 0 for c in chks), (negctl, what)
