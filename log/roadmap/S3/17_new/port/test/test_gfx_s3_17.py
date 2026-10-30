"""Roadmap S3.17: the two drawing calls I14's census still stopped on, and the title menu after a cancelled tournament.

  * showRoutingMapScreen's tee and flag sprites (routing.cpp MapDraw::hole_map): the original's addSprite calls of the sheet
    cells (tee: the last column of sheet 0x18d + clamp(par - 3, 0, 2), row 0, shade 4, flags 0x5e; flag: sheet 0x189 +
    g_courseType, column 0, row 3, shade 2, flags g_courseType + 0x60), drawn at once by the scene family's addSprite body.
    Judged on cov_a_keys `k_dlg_R` (two holes built) against the original's back buffer: without LiveTerrain the frame differs
    only in the two tee-to-green lines (they need the Terrain object's GL context: roadmap S3.8), 246 px = their footprint;
    with LiveTerrain (framegate) in 1 px, one level, on a line. Controls showRoutingMapScreen 11 (tee one pixel right) and 12
    (flag at shade 4) RED; the scene family's mode 12 (addSprite draws nothing) gives S2.4's 370 px back.
  * Surface_createFull_fwd1 (gfx_surface.cpp): mainLoop's forwarder = Surface_createFull(this, a1, a2, 0, a3, a4, a5). The
    body hands that list to Surface_createFull's body (the key's backend entries erased). The argument order is judged by the
    ORIGINAL forwarder under Unicorn (SG_EMU_ORACLE=1); the erase by a unit test with control surface 9 (fwd1 does nothing)
    RED and surface 3 (Surface_createFull keeps the image) carried through the forwarder.
  * the boot's last frame makes the idle passes of a modal loop past its last dispatch (tick.cpp, U-1831): cov_a_champ `n_yes` /
    `end` (the title menu after `Yes, cancel.`) equal the original's back buffer; control tick.frame 13 (no idle passes) RED.
"""
from __future__ import annotations

import csv
import ctypes
import os
import pathlib

import numpy as np
import pytest

from port.test import framegate as FG
from port.test import gfx as GX
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

ROOT_DIR = pathlib.Path(__file__).resolve().parents[2]
NAME_VA = 0x30000000
ROOT = 0x30000800            # the root surface's address given to f_stateptr_4c1570[0] (a key, never dereferenced)
LOCAL = 0xFFF00000           # gfx_local_key(0)
IMG, SPR = LOCAL + 0x6000, LOCAL + 0x6100

KEYS = ("cov_a_keys", "k_dlg_R")
LINES_PX = 246               # k_dlg_R without LiveTerrain: the tee-to-green lines' footprint (no GL context, S3.8)
NO_SPRITES_PX = 370          # S2.4's bound: lines and sprites
HOLES_BOX = (0x184, 0x160, 0x1E0, 0x1A0)   # the two holes' map area (x0, y0, x1, y1)
CHAMP = ("cov_a_champ", ["n_yes", "end"])


@pytest.fixture(scope="module")
def glib():
    lib = L.load()
    if not lib.has("gfx.family.surface"):
        pytest.skip("the library has no surface family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _have(scen, chk):
    return (GX.REFS / scen / "det_b" / f"{chk}.png").exists() and SG.INSTALL.is_dir()


def _negctl(lib, name, mode):
    class _Ctl:
        def __enter__(self):
            self.prev = lib.negctl(name, mode)

        def __exit__(self, *e):
            lib.negctl(name, self.prev or 0)
    return _Ctl()


def _offset(lib, name):
    for n, off, _size in lib.objects():
        if n == name:
            return off
    raise KeyError(name)


def _state(lib):
    st = lib.state()
    st.set_data_base(SG.DATA_VA)
    st.fill_from_exe(True)
    st._namebuf = st.map(NAME_VA, bytearray(0x1000))
    st.write(_offset(lib, "f_stateptr_4c1570"), ROOT.to_bytes(4, "little"))
    st.call("gfx.attach", [])
    return st


def _put_str(st, off, s):
    raw = s.encode() + b"\0"
    ctypes.memmove(ctypes.addressof(st._namebuf) + off, raw, len(raw))
    return NAME_VA + off


def _g(st, name, args=()):
    _put_str(st, 0, name)
    return st.call("gfx.call", [NAME_VA] + [a & 0xFFFFFFFF for a in args] + [0] * (11 - len(args))).eax


def _log(st):
    st.call("gfx.log", [])
    return st.files_read("$gfx/log.txt").decode()


def _pcx(w, h, rows, pal):
    bpl = w + (w & 1)
    hdr = bytearray(0x80)
    hdr[0], hdr[1], hdr[2], hdr[3] = 0xA, 5, 1, 8
    hdr[8:10] = (w - 1).to_bytes(2, "little")
    hdr[10:12] = (h - 1).to_bytes(2, "little")
    hdr[65] = 1
    hdr[66:68] = bpl.to_bytes(2, "little")
    body = bytearray()
    for r in rows:
        for v in list(r) + [0] * (bpl - len(r)):
            body += bytes((0xC1, v)) if v >= 0xC0 else bytes((v,))
    p = bytearray(0x300)
    for k, (r, g, b) in pal.items():
        p[3 * k:3 * k + 3] = bytes((r, g, b))
    return bytes(hdr) + bytes(body) + b"\x0c" + bytes(p)


# ------------------------------------------------------------------------------------------------ the family table
def test_bodies_registered_without_duplicates(glib):
    with _state(glib) as st:
        assert st.call("gfx.check", []).eax == 0, st.files_read("$gfx/bodies.txt")
        _g(st, "Surface_createFull_fwd1", [IMG, 0x320, 0x258, 1, 0, 0])
        log = _log(st)
        assert "handled\tSurface_createFull_fwd1\t1" in log, log
        assert "no body: Surface_createFull_fwd1" not in log, log


# ------------------------------------------------------------------------------------------------ Surface_createFull_fwd1
def _fwd1_then_crop(lib, mode=0, family="gfx.family.surface"):
    """an image loaded into IMG, the forwarder on IMG, then a crop from IMG: the crop finds no image when the key was erased"""
    with _negctl(lib, family, mode), _state(lib) as st:
        st.files_put("a.pcx", _pcx(4, 2, [[1, 1, 1, 1], [1, 1, 1, 1]], {1: (0xF8, 0, 0)}))
        _g(st, "loadImage", [IMG, _put_str(st, 0x100, "a.pcx"), 0, 0, 0x100, 2])
        _g(st, "Surface_blit", [SPR, IMG, 0, 0, 4, 2, 1, 0])
        before = _log(st)
        _g(st, "Surface_createFull_fwd1", [IMG, 0x320, 0x258, 1, 0, 0])
        _g(st, "Surface_blit", [SPR + 0x40, IMG, 0, 0, 4, 2, 1, 0])
        return before, _log(st)


NO_IMAGE = "Surface_blit: a source the backend has no image for"


def test_fwd1_erases_the_key_as_surface_create_full(glib):
    before, after = _fwd1_then_crop(glib)
    assert NO_IMAGE not in before, before          # the image was there for the first crop
    assert NO_IMAGE in after, after                # the forwarder released it (Surface_createFull's Image_releaseAll)


def test_fwd1_negctl_red(glib):
    # control surface 9: the forwarder does nothing, so the second crop still finds the image
    _b, after = _fwd1_then_crop(glib, 9)
    assert NO_IMAGE not in after, after


def test_fwd1_goes_through_surface_create_full(glib):
    # control surface 3 (Surface_createFull keeps the old image) acts through the forwarder: one body for both names
    _b, after = _fwd1_then_crop(glib, 3)
    assert NO_IMAGE not in after, after


def _hooks_addr(name):
    with open(ROOT_DIR / "hooks.csv", newline="", encoding="utf-8") as fh:
        for r in csv.reader(fh):
            if len(r) > 2 and r[1] == "golf_clean.exe" and r[2] == name:
                return int(r[0], 16)
    raise KeyError(name)


@pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") != "1", reason="emulator oracle: SG_EMU_ORACLE=1")
def test_fwd1_argument_order_equals_the_original():
    """the ORIGINAL forwarder under Unicorn, Surface_createFull stubbed as a recorder: it is entered once with ecx unchanged
    and (a1, a2, 0, a3, a4, a5), and the forwarder pops its five arguments and returns Surface_createFull's eax"""
    from port.test import cmdemu as CE
    emu = CE.Emu()
    target = _hooks_addr("Surface_createFull")
    seen = []

    def rec(uc, args, ecx):
        seen.append((ecx, list(args)))
        return 0x5A5A1234
    emu.stub_py(target, 6, rec, pops=True, name="Surface_createFull")
    image = bytes(0x381174)
    for args in ([0x320, 0x258, 1, 0, 0], [0x11, 0x22, 0x33, 0x44, 0x55], [7, 6, 5, 4, 3]):
        seen.clear()
        res = emu.run(_hooks_addr("Surface_createFull_fwd1"), args, image, ecx=0x12345678)
        assert not res.err, res.err
        assert seen == [(0x12345678, [args[0], args[1], 0, args[2], args[3], args[4]])], seen
        assert res.eax == 0x5A5A1234


# ------------------------------------------------------------------------------------------------ the routing map's sprites
def _routing(lib, name=None, mode=0):
    r = GX.render(KEYS[0], [KEYS[1]], lib, negctl=mode, negctl_name=name or "gfx.attach")
    return r, GX.compare(r, KEYS[1])


@pytest.fixture(scope="module")
def routing_run(glib):
    if not _have(*KEYS) or not glib.has("gditext.attach"):
        pytest.skip("no pinned reference / install / host text provider")
    return _routing(glib)


def test_routing_map_differs_only_in_the_hole_lines(routing_run):
    r, c = routing_run
    assert c["total"] == LINES_PX, c
    x0, y0, x1, y1 = c["bbox"]
    assert HOLES_BOX[0] <= x0 and x1 <= HOLES_BOX[2] and HOLES_BOX[1] <= y0 and y1 <= HOLES_BOX[3], c
    assert "handled\taddSprite\t" in r.log, r.log
    assert "no body: showRoutingMapScreen" not in r.log, r.log


@pytest.mark.parametrize("name,mode", [("showRoutingMapScreen", 11), ("showRoutingMapScreen", 12), ("gfx.family.scene", 12)])
def test_routing_sprite_negctl_red(glib, routing_run, name, mode):
    _r, c = _routing(glib, name, mode)
    assert c["total"] > LINES_PX, (name, mode, c)
    if name == "gfx.family.scene":
        assert c["total"] == NO_SPRITES_PX, c


def test_routing_map_with_live_terrain(glib, routing_run):
    # framegate's boot (LiveTerrain attached: the lines are drawn too): 1 px left, one level, on a hole line (17_notes.md)
    row = FG.run_scenario(KEYS[0], [KEYS[1]], glib, probe=False)[0]
    assert row["status"] == "FAIL" and int(row["total_px"]) == 1 and int(row["beyond_px"]) == 0, row
    assert not row["unsupported_before"], row


# ------------------------------------------------------------------------------------------------ the title menu after a cancel
@pytest.fixture(scope="module")
def champ_ok(glib):
    if not all(_have(CHAMP[0], c) for c in CHAMP[1]) or not glib.has("gditext.attach"):
        pytest.skip("no pinned reference / install / host text provider")
    return True


def test_title_menu_after_a_cancelled_tournament(glib, champ_ok):
    rows = FG.run_scenario(CHAMP[0], CHAMP[1], glib, probe=False, live=False)
    for row in rows:
        assert row["status"] == "PASS" and row["boot_status"] == "PASS", row


def test_title_menu_tail_idle_negctl_red(glib, champ_ok):
    rows = FG.run_scenario(CHAMP[0], CHAMP[1], glib, negctl=("tick.frame", 13), probe=False, live=False)
    for row in rows:
        assert row["status"] == "FAIL" and int(row["total_px"]) > 400_000, row
