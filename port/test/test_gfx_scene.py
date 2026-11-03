"""Roadmap S3.5: the graphics backend's `scene` family (port/src/sim/gfx_scene.cpp): the course view's sprites. Notes and the
gate: log/roadmap/S3/5_notes.md.

ORACLES
  blitters   the ORIGINAL jgld bodies (re/match/jgld_blit16b.cpp draw16_100298b0 / draw16_10028630, jgld_blit16.cpp
             draw16_10024140 and the 15 RGB555 bodies of Sprite::drawAlpha) run in an emulator (Unicorn over the user's
             original/jgld.dll) on constructed Sprite / Pal16 / Surface objects, against the port's bodies on the same input
             (gfx.call gfx_scene.blitTest): unscaled and 16.16-stepped scales, clipping on every side, every payload class
  frames     the reference back buffers of the pinned runs (log/c3/alt/refs) through port/test/framegate.py: the pixels whose
             final value a scene draw gave (a run in the family's mask mode 7 against a normal run) must equal the reference
             (log/roadmap/S3/5_sprite_gate.py for the census; one scenario here)
Negative controls: g_negctl_gfx_scene modes 1, 2 (blitters) and 3, 4, 5, 6 (frames) read RED.
"""
from __future__ import annotations

import ctypes
import os
import pathlib
import random
import struct

import numpy as np
import pytest

from port.test import lib as L

pytestmark = pytest.mark.port

ROOT = pathlib.Path(__file__).resolve().parents[2]
JGLD = ROOT / "original" / "jgld.dll"
NAME_VA = 0x30000000
BLK_VA = 0x31000000
ROOT_SURFACE_VA = 0x32000000   # the root surface object's address the test gives Gfx::root_va (nothing is mapped there)

# the bodies (jgld VA, stack argument count) and the port's op code (gfx_scene.cpp b_blit_test)
PLANE = (0x100298B0, 4)
SHADE = (0x10028630, 4)
COLOUR = (0x10024140, 5)
DRAWB = (0x1001BC40, 3)
ALPHA = ["10041f80", "10043430", "10044940", "10045e50", "100473b0", "10048910", "10049ea0", "1004b430", "1004ca10",
         "1004dff0", "1004f5d0", "10050bb0", "100521d0", "100537f0", "10054e20"]
# the same windows' flag-set members (drawAlpha's flag byte stays 1 unless the destination's format word +4 is 1; the
# first call of each arm pair, 0x10018b52 .. 0x100190e7; the root surface's member, roadmap S2.13)
ALPHA_F = ["1002d890", "1002ed60", "10030290", "100317c0", "10032d60", "10034300", "100358b0", "10036e60", "10038460",
           "10039a60", "1003b060", "1003c660", "1003dca0", "1003f2e0", "10040930"]
SCALE_DC0, SCALE_DC4, SCALE_DC8 = 0x10122DC0, 0x10122DC4, 0x10122DC8
INTERSECT_IAT = 0x1012C5E4


# ------------------------------------------------------------------------------------------------ the tables
def color16_planes() -> np.ndarray:
    """build16BitColorTable's five blocks in RGB555 (gfx_scene.h): the same rule, written here from the C3 note c3ad.cpp"""
    t = np.zeros(5 * 0x8000, dtype=np.uint16)
    pack = lambda r, g, b: (((r & 0xff) >> 3) << 10) | (((g & 0xff) >> 3) << 5) | ((b & 0xff) >> 3)
    for k in range(5):
        step = 0x400 + 0x100 * k
        fine = lambda c: (((c // 8) * step) >> 8) & 0xff
        bright = lambda c: min(c + 0x20, 0xff)
        for r in range(0, 256, 8):
            for g in range(0, 256, 8):
                for b in range(0, 256, 8):
                    t[k * 0x8000 + pack(r, g, b)] = pack(fine(r), fine(g), fine(b)) if k < 4 else pack(bright(r), bright(g), bright(b))
    return t


def half_bright() -> np.ndarray:
    i = np.arange(0x10000, dtype=np.uint32)
    return (((((i >> 10) & 0x1e) << 5 | ((i >> 5) & 0x1e)) << 4) | ((i >> 1) & 0xf)).astype(np.uint16)


# ------------------------------------------------------------------------------------------------ the emulated original
class JgldEmu:
    """jgld.dll mapped at its image base in Unicorn; one body called per run on constructed objects."""

    HEAP = 0x20000000
    STACK = 0x00100000
    TRAP = 0x00080000
    SENTINEL = 0x00090000

    def __init__(self):
        import pefile
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
        from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX
        self.UC_X86_REG_ESP, self.UC_X86_REG_EAX = UC_X86_REG_ESP, UC_X86_REG_EAX
        pe = pefile.PE(str(JGLD), fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        img = bytearray(pe.get_memory_mapped_image())
        size = (len(img) + 0xfff) & ~0xfff
        self.mu = mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(self.base, size)
        self.image = bytes(img)
        mu.mem_map(self.HEAP, 0x01000000)
        mu.mem_map(self.STACK, 0x00100000)
        mu.mem_map(self.TRAP, 0x00020000)
        mu.mem_write(self.SENTINEL, b"\xf4")
        # every IAT slot to a trap; IntersectRect (the only import the bodies call) is served by the hook
        pe.parse_data_directories()
        self.traps = {}
        n = 0
        for e in pe.DIRECTORY_ENTRY_IMPORT:
            for i in e.imports:
                a = self.TRAP + 0x10 * n
                n += 1
                struct.pack_into("<I", img, i.address - self.base, a)
                self.traps[a] = (i.name or b"?").decode()
                mu.mem_write(a, b"\xf4")
        self.image = bytes(img)
        mu.hook_add(UC_HOOK_CODE, self._trap, begin=self.TRAP, end=self.TRAP + 0x10000)

    def _trap(self, mu, addr, size, _):
        name = self.traps.get(addr)
        esp = mu.reg_read(self.UC_X86_REG_ESP)
        ret, dst, a, b = struct.unpack("<IIII", mu.mem_read(esp, 16))
        if name != "IntersectRect":
            raise RuntimeError(f"import {name} called")
        ra = struct.unpack("<4i", mu.mem_read(a, 16))
        rb = struct.unpack("<4i", mu.mem_read(b, 16))
        r = (max(ra[0], rb[0]), max(ra[1], rb[1]), min(ra[2], rb[2]), min(ra[3], rb[3]))
        ok = r[0] < r[2] and r[1] < r[3]
        mu.mem_write(dst, struct.pack("<4i", *(r if ok else (0, 0, 0, 0))))
        mu.reg_write(self.UC_X86_REG_EAX, 1 if ok else 0)
        mu.reg_write(self.UC_X86_REG_ESP, esp + 16)
        mu.reg_write(__import__("unicorn").x86_const.UC_X86_REG_EIP, ret)

    def run(self, body, nargs, spr: bytes, w, h, pal: np.ndarray, dst: np.ndarray, dw, dh, x, y, scale, p4, p5, table):
        mu = self.mu
        mu.mem_write(self.base, self.image)          # fresh data (the bodies park their loop words in module globals)
        mu.ctl_flush_tb()                            # the stubs below are rewritten per run: drop translated code
        H = self.HEAP
        cur = [H + 0x1000]

        def alloc(data: bytes, align=16):
            a = (cur[0] + align - 1) & ~(align - 1)
            mu.mem_write(a, data)
            cur[0] = a + len(data)
            return a

        def stub(code: bytes):
            return alloc(code)

        ret_imm = lambda v: stub(b"\xb8" + struct.pack("<I", v) + b"\xc3")
        bits = alloc(dst.astype("<u2").tobytes())
        clip = alloc(struct.pack("<4i", 0, 0, dw, dh))
        fmt = alloc(struct.pack("<2i", 16, 1))
        tab555 = alloc(pal.astype("<u2").tobytes())
        surf_vt = [ret_imm(0)] * 60
        surf_vt[4] = stub(b"\xb8\x01\x00\x00\x00\xc3")      # lock
        surf_vt[8] = ret_imm(bits)                            # bits
        surf_vt[9] = stub(b"\xc2\x04\x00")                    # unlock(int)
        surf_vt[51] = ret_imm(clip)                           # clipRect
        surf_vt[56] = ret_imm(dw)                             # pitch (words)
        surf_vt[57] = ret_imm(fmt)                            # format
        svt = alloc(struct.pack("<60I", *surf_vt))
        surf = alloc(struct.pack("<I", svt) + b"\0" * 0x100)
        pvt_l = [ret_imm(0)] * 12
        pvt_l[6] = ret_imm(tab555)
        pvt_l[7] = ret_imm(tab555)
        pvt = alloc(struct.pack("<12I", *pvt_l))
        palobj = alloc(struct.pack("<I", pvt) + b"\0" * 0x40)
        sbits = alloc(spr)
        sprite = bytearray(0x60)
        struct.pack_into("<I", sprite, 0x10, palobj)
        struct.pack_into("<I", sprite, 0x14, sbits)
        struct.pack_into("<iii", sprite, 0x2c, w, w, h)
        sp = alloc(bytes(sprite))
        tab = alloc(table.astype("<u2").tobytes()) if table is not None else 0
        mu.mem_write(SCALE_DC0, struct.pack("<iii", *scale))
        args = [surf, x & 0xffffffff, y & 0xffffffff, (tab if table is not None else p4) & 0xffffffff, p5 & 0xffffffff][:nargs]
        esp = self.STACK + 0xF0000
        stack = struct.pack(f"<I{nargs}I", self.SENTINEL, *args)
        esp -= len(stack)
        mu.mem_write(esp, stack)
        from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EBP
        mu.reg_write(self.UC_X86_REG_ESP, esp)
        mu.reg_write(UC_X86_REG_ECX, sp)
        mu.reg_write(UC_X86_REG_EBP, 0)
        mu.emu_start(body, self.SENTINEL, count=50_000_000)
        out = np.frombuffer(bytes(mu.mem_read(bits, dw * dh * 2)), dtype="<u2").copy()
        return out


@pytest.fixture(scope="module")
def emu():
    if not JGLD.exists():
        pytest.skip("no original/jgld.dll")
    try:
        import unicorn  # noqa: F401
    except ImportError:
        pytest.skip("unicorn not installed")
    return JgldEmu()


@pytest.fixture(scope="module")
def slib():
    lib = L.load()
    if not lib.has("gfx.family.scene"):
        pytest.skip("the library has no scene family")
    return lib


@pytest.fixture(scope="module")
def tables():
    return {"planes": color16_planes(), "half": half_bright()}


def port_blit(lib, op, extra, spr: bytes, w, h, pal, dst, dw, dh, x, y, scale, negctl=0):
    with lib.state() as st:
        st.call("gfx.attach", [])
        if negctl:
            lib.negctl("gfx.family.scene", negctl)
        try:
            if op >= 0x40:
                # the ops through registered bodies draw on the root surface, whose address is the GameState word
                # f_stateptr_4c1570[0] (Gfx::root_va); a fresh state holds 0 there, which the bodies read as no surface
                off = {n: o for n, o, _s in lib.objects()}["f_stateptr_4c1570"]
                st.write(off, struct.pack("<I", ROOT_SURFACE_VA))
            name = st.map(NAME_VA, bytearray(0x1000))
            raw = b"gfx_scene.blitTest\0"
            ctypes.memmove(ctypes.addressof(name), raw, len(raw))
            blk = bytearray(spr) + bytearray(-len(spr) % 16)
            pal_off = len(blk)
            blk += pal.astype("<u2").tobytes()
            dst_off = len(blk)
            blk += dst.astype("<u2").tobytes()
            st.map(BLK_VA, blk)
            a = [(op | (extra << 16)) & 0xffffffff, BLK_VA, w | (h << 16), BLK_VA + pal_off, BLK_VA + dst_off, dw | (dh << 16),
                 x & 0xffffffff, y & 0xffffffff, scale[0] & 0xffffffff, scale[1] & 0xffffffff, scale[2] & 0xffffffff]
            st.call("gfx.call", [NAME_VA] + a)
            raw = st.files_read("$gfx/scene_blit.bin")
        finally:
            if negctl:
                lib.negctl_reset()
    return np.frombuffer(raw, dtype="<u2").copy()


SCALES = [(4, 4, 4), (1, 1, 1), (16, 16, 16), (2, 2, 4), (3, 3, 4), (8, 8, 4), (6, 6, 4), (12, 12, 16), (16, 16, 13),
          (16, 16, 10), (5, 7, 16), (32, 32, 16), (1, 1, 3)]


def cases(seed: int, n: int):
    rnd = random.Random(seed)
    for k in range(n):
        w, h = rnd.randint(1, 24), rnd.randint(1, 20)
        cls = rnd.random()
        spr = bytes(rnd.choice([rnd.randint(0, 0xf7), 0xff, rnd.randint(0xf8, 0xfc)] if cls < 0.6 else
                               [rnd.randint(0, 0xff)]) for _ in range(w * h))
        dw, dh = rnd.randint(8, 40), rnd.randint(8, 30)
        x, y = rnd.randint(-w, dw), rnd.randint(-h, dh)
        scale = SCALES[k % len(SCALES)]
        yield spr, w, h, dw, dh, x, y, scale, rnd


def check(emu, lib, tables, op, body, extra, seed, n, negctl=0):
    bad = 0
    for spr, w, h, dw, dh, x, y, scale, rnd in cases(seed, n):
        if op == 0:
            # payloads 0xfd / 0xfe name planes 5 and 6, past build16BitColorTable's five blocks: the original reads whatever
            # follows the table (U-1343), the port reports the case; the oracle compares the five modelled planes
            spr = bytes(0xfc if c in (0xfd, 0xfe) else c for c in spr)
        pal = np.array([rnd.randint(0, 0x7fff) for _ in range(256)], dtype=np.uint16)
        dst = np.array([rnd.randint(0, 0x7fff) for _ in range(dw * dh)], dtype=np.uint16)
        table = tables["planes"] if op in (0,) else tables["half"] if op in (1, 0x60) else None
        if op == 3:
            spr = bytes(c if c < 0xfe or rnd.random() < 0.5 else c for c in spr)
        epal = pal ^ 0x7fff if op == 0x51 else pal      # 0x51: the port's sprite holds the complement as its own palette
        want = emu.run(body[0], body[1], spr, w, h, epal, dst, dw, dh, x, y, scale, extra, 0xf8, table)
        got = port_blit(lib, op, extra, spr, w, h, pal, dst, dw, dh, x, y, scale, negctl)
        if not np.array_equal(want, got):
            bad += 1
    return bad


@pytest.mark.parametrize("seed", [1, 2])
def test_plane_blit_equals_the_original(emu, slib, tables, seed):
    assert check(emu, slib, tables, 0, PLANE, 0, seed, 40) == 0


@pytest.mark.parametrize("seed", [3])
def test_shade_blit_equals_the_original(emu, slib, tables, seed):
    assert check(emu, slib, tables, 1, SHADE, 0, seed, 40) == 0


@pytest.mark.parametrize("colour", [0x7c00, 0x7fff, 0x1234])
def test_colour_blit_equals_the_original(emu, slib, tables, colour):
    assert check(emu, slib, tables, 2, COLOUR, colour, 10 + colour, 26) == 0


@pytest.mark.parametrize("seed", [20, 21])
def test_drawb_equals_the_original(emu, slib, tables, seed):
    assert check(emu, slib, tables, 3, DRAWB, 0, seed, 40) == 0


@pytest.mark.parametrize("k", range(len(ALPHA)))
@pytest.mark.parametrize("flag", [0, 1])
def test_alpha_bodies_equal_the_original(emu, slib, tables, k, flag):
    assert check(emu, slib, tables, 0x10 + k, (int(ALPHA[k], 16), 4), flag, 100 + k, 13) == 0


@pytest.mark.parametrize("k", range(len(ALPHA_F)))
@pytest.mark.parametrize("flag", [0, 1])
def test_alpha_flagset_bodies_equal_the_original(emu, slib, tables, k, flag):
    assert check(emu, slib, tables, 0x10 + 15 + k, (int(ALPHA_F[k], 16), 4), flag, 300 + k, 13) == 0


# the one registered Surface_drawSprite body (gfx_reports.cpp's, roadmap S2.13, with 5_merge.md's hand-over to
# gfxs::surface_draw_sprite) on the root surface at the centre of alpha window k: equal to the window's flag-set member
@pytest.mark.parametrize("k", range(len(ALPHA_F)))
@pytest.mark.parametrize("flag", [0, 1])
def test_surface_drawsprite_body_equals_the_original(emu, slib, tables, k, flag):
    assert check(emu, slib, tables, 0x40 + k, (int(ALPHA_F[k], 16), 4), flag, 400 + k, 13) == 0


# Image_draw with a palette object as its mask (drawB's key for the call) and without one (the sprite's own palette)
@pytest.mark.parametrize("op", [0x50, 0x51])
def test_image_draw_mask_equals_the_original(emu, slib, tables, op):
    assert check(emu, slib, tables, op, DRAWB, 0, 500 + op, 40) == 0


# the addSprite body (S2.10's screens): shade 0, flags -1 = drawQueuedSprite's Surface_blit2 arm (drawG, half-bright table)
def test_add_sprite_body_equals_the_original(emu, slib, tables):
    assert check(emu, slib, tables, 0x60, SHADE, 0, 600, 40) == 0


@pytest.mark.parametrize("mode,op,body", [(9, 0x40 + 1, "1002ed60"), (9, 0x40 + 6, "100358b0"), (10, 0x50, None), (12, 0x60, None)])
def test_scene_body_negctl_red(emu, slib, tables, mode, op, body):
    b = (int(body, 16), 4) if body else (DRAWB if op == 0x50 else SHADE)
    assert check(emu, slib, tables, op, b, 1 if body else 0, 700 + mode, 40, negctl=mode) > 0


@pytest.mark.parametrize("mode", [1, 2])
def test_blitter_negctl_red(emu, slib, tables, mode):
    assert check(emu, slib, tables, 0, PLANE, 0, 1, 40, negctl=mode) > 0


# ------------------------------------------------------------------------------------------------ frames
def _frames(scenario: str, checkpoints: list[str], negctl: int, tmp: pathlib.Path):
    from PIL import Image
    from port.test import framegate as FG
    d = tmp / f"{scenario}_{negctl}"
    FG.run_scenario(scenario, checkpoints=checkpoints, images=d, probe=False,
                    negctl=("gfx.family.scene", negctl) if negctl else None)
    return {c: np.asarray(Image.open(d / f"{scenario}.{c}.port.png").convert("RGB")) for c in checkpoints}


def _view():
    from port.test import framegate as FG
    lab, names = FG.label_map("ingame")
    return lab == names.index("view")


FRAME_CASES = [("sandbox_basic", ["sandbox_00", "sandbox_20", "sandbox_40"])]


@pytest.fixture(scope="module")
def frame_runs(slib, tmp_path_factory):
    if not (ROOT / "log" / "c3" / "alt" / "refs" / "sandbox_basic").exists():
        pytest.skip("no reference frames")
    tmp = tmp_path_factory.mktemp("scene_frames")
    out = {}
    for s, cks in FRAME_CASES:
        out[(s, 0)] = _frames(s, cks, 0, tmp)
        out[(s, 7)] = _frames(s, cks, 7, tmp)
    return out, tmp


@pytest.mark.parametrize("scenario,chk", [(s, c) for s, cks in FRAME_CASES for c in cks])
def test_scene_pixels_equal_the_original(frame_runs, scenario, chk):
    """every view pixel whose final value a scene draw gave equals the reference; a one-level difference is allowed only where
    the draw blends with the destination (shadow planes over terrain the rasteriser gives within G2)"""
    from port.test import framegate as FG
    runs, _ = frame_runs
    a, b = runs[(scenario, 0)][chk], runs[(scenario, 7)][chk]
    ref = FG.GX.reference(scenario, chk)
    mask = (a != b).any(axis=2) & _view()
    d5 = np.abs((a.astype(np.int16) >> 3) - (ref.astype(np.int16) >> 3)).max(axis=2)
    assert mask.sum() > 50000
    assert int((mask & (d5 > 1)).sum()) == 0
    assert int((mask & (d5 == 1)).sum()) <= 10


# the frame-level controls: each changes the view pixels of its checkpoint against mode 0 (3: a golfer's palette layers, 4:
# scaled queue draws at a zoomed-out checkpoint, 6: the golfers' name labels)
FRAME_NEGCTL = [(3, "sandbox_basic", "sandbox_20"), (4, "cov_a_golfer", "end"), (6, "cov_a_golfer", "holes")]


@pytest.mark.parametrize("mode,scenario,chk", FRAME_NEGCTL)
def test_scene_frame_negctl_red(frame_runs, mode, scenario, chk):
    from port.test import framegate as FG
    runs, tmp = frame_runs
    base = runs[(scenario, 0)][chk] if (scenario, 0) in runs and chk in runs[(scenario, 0)] else _frames(scenario, [chk], 0, tmp)[chk]
    neg = _frames(scenario, [chk], mode, tmp)[chk]
    ref = FG.GX.reference(scenario, chk)
    view = _view()
    base_bad = int(((base != ref).any(axis=2) & view).sum())
    neg_bad = int(((neg != ref).any(axis=2) & view).sum())
    assert neg_bad > base_bad


# ------------------------------------------------------------------------------------------------ cells without a sprite
# A queue draw of a sheet cell flic::sheet_to_gfx kept no sprite for draws nothing (gfx_scene.cpp sprite_of). The family records,
# at each such key's first draw, the loaded sheets that held the cell then (sheet, index, first cell, path). Two checks:
#   1 the cut: the sheet's flic, loaded again through flic.load with its loadWorldAssets call (menu_flics.inc), gives that index a
#     result other than 0 (nothing kept; S3.4's test_flic.py judges those cuts against the original's sheet memory);
#   2 the original's record: in every segment start image where the original has the same sheet at the same first cell, the
#     SpriteRef there holds no pixels (handle NULL, or a trimmed width +0x10 / height +0x14 of 0). Images with another layout
#     (the sheets move when loadWorldAssets runs again) say nothing about the draw and are skipped.
GS_VA = 0x004C1000
CELL_POOL_VA = 0x005AAA30
CELL_SIZE = 0x2C
SHEET_FIRST_VA = 0x005A9370      # g_sheetFirstCell
SHEET_COUNT = 590

NO_SPRITE_WHERE: dict[int, str] = {}   # the port's loaded sheets holding each key at its first draw (dumpNoSprite)


def no_sprite_keys(lib, scenario: str, max_records: int | None = None, negctl: int = 0) -> dict[int, int]:
    from port.test import boot as B
    keys: dict[int, int] = {}

    def before(st):
        st.call("gfx.attach", [])
        if negctl:
            lib.negctl("gfx.family.scene", negctl)

    def after(st, _res):
        name = st.map(NAME_VA, bytearray(0x1000))
        raw = b"gfx_scene.dumpNoSprite\0"
        ctypes.memmove(ctypes.addressof(name), raw, len(raw))
        st.call("gfx.call", [NAME_VA] + [0] * 11)
        txt = st.files_read("$gfx/scene_nosprite.txt") or b""
        for ln in txt.decode("latin-1").splitlines():
            k, n, where = ln.split()
            keys[int(k)] = int(n)
            NO_SPRITE_WHERE[int(k)] = where
        st.call("gfx.detach", [])

    try:
        B.run(scenario, lib, max_records=max_records, before=before, after=after)
    finally:
        if negctl:
            lib.negctl_reset()
    return keys


def parse_where(where: str) -> list[dict]:
    if where == "-":
        return []
    out = []
    for e in where.split(","):
        sheet, idx, first, have, path = e.split(":", 4)
        out.append({"sheet": int(sheet), "idx": int(idx), "first": int(first), "have": have,
                    "path": path.replace("*", " ")})
    return out


def flic_calls(lib) -> dict[str, list[tuple[int, int, int]]]:
    """path -> [(frames, delay, flag)] of the loadFlic calls of loadWorldAssets (menu_flics.inc) and loadInterfaceArt
    (menu_iart.inc's IaFlic rows), the strings read from the exe"""
    import re
    objs = {n: off for n, off, _sz in lib.objects()}
    if not lib.exe_loaded():
        lib.exe_load()
    out: dict[str, list[tuple[int, int, int]]] = {}
    sim = ROOT / "port" / "src" / "sim"
    pats = [(sim / "menu_flics.inc", r"\{0x[0-9a-f]+, GSO\((\w+), (0x[0-9a-f]+)\), (-?\d+), (-?\d+), (-?\d+)\}"),
            (sim / "menu_iart.inc", r"\{IaFlic, 0x[0-9a-f]+, GSO\((\w+), (0x[0-9a-f]+)\), 0x[0-9a-f]+, (-?\d+), (-?\d+), (-?\d+)\}")]
    with lib.state() as st:
        st.fill_from_exe()
        for path, pat in pats:
            for m in re.finditer(pat, path.read_text()):
                p = st.read(objs[m[1]] + int(m[2], 16), 80).split(b"\0")[0].decode("latin-1")
                out.setdefault(p, []).append((int(m[3]), int(m[4]), int(m[5])))
    return out


def recut(lib, path: str, calls) -> list[list[int]]:
    """the cell results of every loadWorldAssets call of `path` (one list per distinct call)"""
    from port.test import test_flic as TF
    port = TF.Port(lib)
    try:
        return [[c["result"] for c in port.load(path, fr, dl, fl)["cells"]] for fr, dl, fl in sorted(set(calls))]
    finally:
        port.close()


def original_cell(scenario: str, key: int, sheet: int | None = None, first: int | None = None) -> list[tuple[int, int, int]]:
    """(handle, trimmed width, trimmed height) of the cell at `key` in the segment start images of `scenario` (with `sheet`:
    only the images where the original's g_sheetFirstCell[sheet] == first)"""
    out = []
    for p in sorted((ROOT / "port" / "corpus" / "segments" / scenario).glob("*/start.data.bin")):
        d = p.read_bytes()
        if sheet is not None and struct.unpack_from("<i", d, SHEET_FIRST_VA - GS_VA + 4 * sheet)[0] != first:
            continue
        o = key - GS_VA
        out.append(struct.unpack_from("<I", d, o + 4) + struct.unpack_from("<ii", d, o + 0x10))
    return out


def judge_no_sprite(lib, scenario: str, keys: dict[int, int], calls) -> dict:
    """per key: cut (every holding sheet's recut gives a non-zero result there), direct (records compared), bad (lines)"""
    res = {"keys": len(keys), "cut_ok": 0, "direct": 0, "bad": [], "no_sheet": 0}
    cache: dict[str, list[list[int]]] = {}
    for key in sorted(keys):
        c = (key - CELL_POOL_VA) // CELL_SIZE
        ents = parse_where(NO_SPRITE_WHERE.get(key, "-"))
        if not ents:
            res["no_sheet"] += 1
        ok = True
        for e in ents:
            if e["path"] not in calls:
                res["bad"].append(f"{c}: sheet {e['sheet']} path {e['path']!r} is no loadWorldAssets call")
                ok = False
                continue
            if e["path"] not in cache:
                cache[e["path"]] = recut(lib, e["path"], calls[e["path"]])
            if not any(e["idx"] < len(r) and r[e["idx"]] != 0 for r in cache[e["path"]]):
                res["bad"].append(f"{c}: sheet {e['sheet']} {e['path']!r} index {e['idx']} cuts pixels")
                ok = False
            for handle, w, h in original_cell(scenario, key, e["sheet"], e["first"]):
                res["direct"] += 1
                if not (handle == 0 or w == 0 or h == 0):
                    res["bad"].append(f"{c}: the original's record holds {w}x{h} (handle {handle:#x})")
                    ok = False
        res["cut_ok"] += 1 if ok and ents else 0
    return res


NO_SPRITE_CASES = ["cov_k1_paths"]


@pytest.mark.parametrize("scenario", NO_SPRITE_CASES)
def test_cells_without_sprite_are_empty(slib, scenario):
    if not list((ROOT / "port" / "corpus" / "segments" / scenario).glob("*/start.data.bin")):
        pytest.skip("no segment start images")
    keys = no_sprite_keys(slib, scenario)
    assert keys, "the scenario draws no cell without a sprite (the case lost its point)"
    calls = flic_calls(slib)
    r = judge_no_sprite(slib, scenario, keys, calls)
    assert not r["bad"], r["bad"]
    assert r["cut_ok"] == r["keys"] and r["direct"] > 0, r
    # the checker's control: a cell of the same sheet that does cut pixels is reported as bad
    key = sorted(keys)[0]
    e = parse_where(NO_SPRITE_WHERE[key])[0]
    cut = recut(slib, e["path"], calls[e["path"]])[0]
    j = next(i for i, v in enumerate(cut) if v == 0)
    k2 = CELL_POOL_VA + CELL_SIZE * (e["first"] + j)
    NO_SPRITE_WHERE[k2] = f"{e['sheet']}:{j}:{e['first']}:-:{e['path'].replace(' ', '*')}"
    assert judge_no_sprite(slib, scenario, {k2: 1}, calls)["bad"]


# ------------------------------------------------------------------------------------------------ the standing figure (S3.5b)
# updateVipResident's queue entry (enqueueSprite(sx, sy, sy, 0x1e, 0x28, cell, 4, (n + 0x9c) | 0x100), updateVipResident+0x4018dc): the
# standing figure of a live animal record, drawn with the golfer palette past the golfer table. The exact oracle is the original's
# root surface at renderSpriteQueue's return (port/corpus/scene/<scenario>/<checkpoint>.z, log/roadmap/S3/5_tools/scene_rec.py).
# Before the entry was queued the figure was missing: cov_k1_animals bought / menu2 187 and 216 px beyond one level in the box
# below (5_queue_w3.tsv), cov_b_inject end 208 px in its own box.
FIGURE_CASES = [("cov_k1_animals", "bought", (405, 440, 320, 365)), ("cov_k1_animals", "menu2", (405, 440, 320, 365)),
                ("cov_b_inject", "end", (435, 470, 300, 345))]


def _queue_root(lib, scenario: str, ordn: int, negctl: int):
    """the port's root surface at renderSpriteQueue call `ordn` (one boot, gfx + LiveTerrain attached)"""
    import ctypes as C
    from port.test import boot as B
    from port.test import framegate as FG
    from port.test import terrainlive as TLm
    got = {}
    va = FG.terrain_address(scenario)
    if getattr(lib, "dll", None) is not None:
        FG._bind_live(lib.dll)

    def before(st):
        st.call("gfx.attach", [])
        nm = st.map(0x30000000, bytearray(0x1000))
        raw = b"gfx_scene.captureQueueAt" + bytes([0])
        C.memmove(C.addressof(nm), raw, len(raw))
        st.call("gfx.call", [0x30000000, ordn] + [0] * 10)
        if TLm.live_attach(lib, st, va) != 0:
            raise RuntimeError("sg_ts_live_attach failed")

    def after(st, _res):
        raw = st.files_read(f"$gfx/queue_{ordn}.555")
        got["root"] = None if raw is None else np.frombuffer(raw, "<u2").reshape(FG.H, FG.W).copy()
        st.call("gfx.detach", [])
        TLm.live_detach(lib, st)

    if negctl:
        lib.negctl("gfx.family.scene", negctl)
    try:
        B.run(scenario, lib, max_records=ordn + 3, before=before, after=after, to_end=False)
    finally:
        if negctl:
            lib.negctl_reset()
    return got["root"]


def _figure_bad(lib, scenario: str, checkpoint: str, box, negctl: int) -> int:
    import json
    import zlib
    d = ROOT / "port" / "corpus" / "scene" / scenario
    if not (d / "index.json").exists() or not (d / f"{checkpoint}.z").exists():
        pytest.skip("no recorded queue oracle for " + scenario)
    meta = json.loads((d / "index.json").read_text())["checkpoints"][checkpoint]
    orig = np.frombuffer(zlib.decompress((d / f"{checkpoint}.z").read_bytes()), "<u2").reshape(600, 800)
    root = _queue_root(lib, scenario, meta["ord"], negctl)
    x0, x1, y0, y1 = box
    ch = lambda v, s: ((v >> s) & 0x1f).astype(np.int16)
    d5 = np.maximum.reduce([np.abs(ch(root, s) - ch(orig, s)) for s in (0, 5, 10)])
    return int((d5[y0:y1, x0:x1] > 1).sum())


@pytest.mark.parametrize("scenario,checkpoint,box", FIGURE_CASES)
def test_standing_figure_equals_the_original(slib, scenario, checkpoint, box):
    """the box around the animal record's figure has no pixel beyond one level of the original's root at the queue's return"""
    assert _figure_bad(slib, scenario, checkpoint, box, 0) == 0


@pytest.mark.parametrize("scenario,checkpoint,box", FIGURE_CASES[:1])
def test_standing_figure_negctl_red(slib, scenario, checkpoint, box):
    """control 13: an entry past the golfer table (a8 & 0x1ff >= 0x19c) draws nothing, as the port did before the queue entry
    existed: the figure's pixels differ from the original's"""
    assert _figure_bad(slib, scenario, checkpoint, box, 13) > 100
