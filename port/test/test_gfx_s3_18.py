"""Roadmap S3.18: the bulletin board's photos (gfx_bulletin.cpp `showBulletinBoard.capture` / `.photo`).

showBulletinBoard (c3em.cpp ShowBulletinBoard_re) takes a photo of the root surface on its first call with a pending photo
position: saveBmp(root, "snapshots\\accomp<index>.bmp", the 0xc8 x 0xa0 rectangle at clamp(sx - 0x64, 0, 0x258),
clamp(sy - 0x64, 0, 0x1b8)), and the board loads every listed event's file back into ONE photo Image (loadBmpSurface) and
blits it with Surface_blitThunk. The shipped install has no `snapshots` folder, so the original's fopen("wb") fails and no
photo is ever drawn in the pinned scenarios (U-1840); these tests give the files a `snapshots` directory.

  * the capture's file equals the ORIGINAL saveBmp's under Unicorn (SG_EMU_ORACLE=1) over the port's own root surface, and a
    Python model of c3ea.cpp SaveBmp_re by default; no directory, no file;
  * the photo's pixels equal the ORIGINAL loadBmpSurface's under Unicorn (24-bit bottom-up, 24-bit top-down, 8-bit) and the
    model by default; the capture loaded back is the root rectangle; a missing file redraws the previous event's photo
    (the board's one Image), and no surface draws nothing;
  * controls gfx.family.bulletin 5..10 RED.
"""
from __future__ import annotations

import ctypes
import os
import random
import struct

import numpy as np
import pytest

from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

NAME_VA = 0x30000000
ROOT = 0x30000800            # the root surface's key (f_stateptr_4c1570[0]); never dereferenced
LOCAL = 0xFFF00000           # gfx_local_key(0)
IMG = LOCAL + 0x6200
SNAP = "snapshots\\accomp"
ORACLE = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") != "1", reason="emulator oracle: SG_EMU_ORACLE=1")


@pytest.fixture(scope="module")
def glib():
    lib = L.load()
    if not lib.has("gfx.family.bulletin"):
        pytest.skip("the library has no bulletin family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


class _Negctl:
    def __init__(self, lib, mode):
        self.lib, self.mode = lib, mode

    def __enter__(self):
        self.prev = self.lib.negctl("gfx.family.bulletin", self.mode)

    def __exit__(self, *e):
        self.lib.negctl("gfx.family.bulletin", self.prev or 0)


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


def _root(st):
    assert st.call("gfx.dump", [1]).eax == 0
    return np.frombuffer(st.files_read("$gfx/root.555"), dtype="<u2").reshape(600, 800).copy()


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


def _pattern_root(st, seed):
    """the root surface filled with a pseudo-random 800 x 600 picture (loadImage of a PCX, then Surface_blitThunk)"""
    rnd = random.Random(seed)
    rows = [[rnd.randrange(256) for _ in range(800)] for _ in range(600)]
    pal = {k: (rnd.randrange(256), rnd.randrange(256), rnd.randrange(256)) for k in range(256)}
    st.files_put("pattern.pcx", _pcx(800, 600, rows, pal))
    assert _g(st, "loadImage", [IMG, _put_str(st, 0x100, "pattern.pcx"), 0, 0, 0x100, 2]) == 0 or True
    _g(st, "Surface_blitThunk", [IMG, ROOT, 0, 0, 0, 0, 800, 600])
    root = _root(st)
    assert len(np.unique(root)) > 200, "the pattern did not reach the root"
    return root


def _clamp(v, lo, hi):
    return lo if v < lo else (hi if v > hi else v)


def _save_bmp_model(root, sx, sy):
    """c3ea.cpp SaveBmp_re over the root rectangle the board computes (display format RGB555)"""
    bx, by = _clamp(sx - 0x64, 0, 0x258), _clamp(sy - 0x64, 0, 0x1B8)
    w, h = 0xC8, 0xA0
    rows = []
    for y in range(by, by + h):
        row = bytearray()
        for x in range(bx, bx + w):
            p = int(root[y, x])
            row += bytes(((p << 3) & 0xFF, (p >> 2) & 0xF8, (p >> 7) & 0xF8))
        rows.append(bytes(row))
    bits = b"".join(reversed(rows))   # flipBmpRows: a positive height is stored bottom-up
    info = struct.pack("<IiiHHIIiiII", 0x28, w, h, 1, 0x18, 0, 0, 0, 0, 0, 0)
    return b"BM" + struct.pack("<IHHI", len(bits) + 0x36, 0, 0, 0x36) + info + bits


def _bmp(w, h, pixels, bpp=24, top_down=False, table=None):
    """a BMP as a tool writes it: rows bottom-up unless top_down (negative height); no row padding (the original reads none)"""
    rows = [pixels[r] for r in range(h)]
    if not top_down:
        rows = rows[::-1]
    if bpp == 24:
        bits = b"".join(b"".join(bytes((b, g, r)) for (r, g, b) in row) for row in rows)
        off, extra = 0x36, b""
    else:
        bits = b"".join(bytes(row) for row in rows)
        extra = b"".join(bytes((b, g, r, 0)) for (r, g, b) in table)
        off = 0x36 + len(extra)
    info = struct.pack("<IiiHHIIiiII", 0x28, w, -h if top_down else h, 1, bpp, 0, 0, 0, 0, 0, 0)
    return b"BM" + struct.pack("<IHHI", off + len(bits), 0, 0, off) + info + extra + bits


def _decode_model(bmp):
    """c3ee.cpp LoadBmpSurface_re: the words loadBmpSurface stores, as rows top-down (w, h, words)"""
    off = struct.unpack_from("<I", bmp, 10)[0]
    w, hh = struct.unpack_from("<ii", bmp, 0xE + 4)
    bpp = struct.unpack_from("<H", bmp, 0xE + 0xE)[0]
    h = abs(hh)
    bits = bytearray(bmp[off:])
    bpr = w * (3 if bpp == 24 else 1)
    rows = [bits[r * bpr:(r + 1) * bpr] for r in range(h)]
    if hh > 0:
        rows = rows[::-1]
    out = np.zeros((h, w), dtype=np.uint16)
    for r in range(h):
        for c in range(w):
            if bpp == 8:
                e = 0x36 + 4 * rows[r][c]
                b, g, rr = bmp[e], bmp[e + 1], bmp[e + 2]
            else:
                b, g, rr = rows[r][3 * c], rows[r][3 * c + 1], rows[r][3 * c + 2]
            out[r, c] = ((rr >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
    return out


def _capture(st, sx, sy, index):
    return _g(st, "showBulletinBoard.capture", [sx, sy, _put_str(st, 0x200, str(index))])


def _photo(st, lib, x, y, drawn, name):
    st.write(_offset(lib, "g_textBuffer"), name.encode() + b"\0")
    _g(st, "showBulletinBoard.photo", [x, y, drawn])


def _board(st):
    _g(st, "showBulletinBoard.board", [])


# ------------------------------------------------------------------------------------------------ the family table
def test_bodies_registered_without_duplicates(glib):
    with _state(glib) as st:
        assert st.call("gfx.check", []).eax == 0, st.files_read("$gfx/bodies.txt")
        _capture(st, 400, 300, 3)
        _photo(st, glib, 100, 300, 1, SNAP + "3.bmp")
        log = _log(st)
        assert "handled\tshowBulletinBoard.capture\t1" in log, log
        assert "handled\tshowBulletinBoard.photo\t1" in log, log
        assert "a photo (loadBmpSurface" not in log, log


# ------------------------------------------------------------------------------------------------ the capture (saveBmp)
CAPTURES = [(400, 300, 0), (10, 5, 7), (790, 590, 12), (0x64 + 0x258 + 1, 0x64 + 0x1B8 + 50, 21), (-500, -500, 3)]


def _captured(lib, sx, sy, index, mode=0, seed=1, with_dir=True):
    with _Negctl(lib, mode), _state(lib) as st:
        if with_dir:
            st.files_put("snapshots/readme.txt", b"x")
        root = _pattern_root(st, seed)
        _capture(st, sx, sy, index)
        return root, st.files_read(f"{SNAP}{index}.bmp")


@pytest.mark.parametrize("sx,sy,index", CAPTURES)
def test_capture_equals_the_save_bmp_model(glib, sx, sy, index):
    root, f = _captured(glib, sx, sy, index, seed=index + 1)
    assert f == _save_bmp_model(root, sx, sy)


def test_no_snapshots_directory_writes_nothing(glib):
    _root, f = _captured(glib, 400, 300, 4, with_dir=False)
    assert f is None


@pytest.mark.parametrize("mode", [5, 6])
def test_capture_negctl_red(glib, mode):
    root, f = _captured(glib, 400, 300, 2, mode=mode, seed=9)
    assert f is not None and f != _save_bmp_model(root, 400, 300)


def test_capture_directory_negctl_red(glib):
    # control 7: a missing directory is ignored, so a file appears where the original's fopen fails
    _root, f = _captured(glib, 400, 300, 4, mode=7, with_dir=False)
    assert f is not None


@pytest.mark.parametrize("with_dir", [True, False])
def test_capture_writes_no_gamestate_byte(glib, with_dir):
    """the capture (and the photo that reads its file back) leaves every GameState byte as it was; the file appears only with
    a `snapshots` directory"""
    with _state(glib) as st:
        if with_dir:
            st.files_put("snapshots/readme.txt", b"x")
        _pattern_root(st, 4)
        size = 0x381174
        before = st.read(0, size)
        _capture(st, 400, 300, 6)
        after_capture = st.read(0, size)
        _board(st)
        _photo(st, glib, 100, 400, 1, SNAP + "6.bmp")
        st.write(_offset(glib, "g_textBuffer"), before[_offset(glib, "g_textBuffer"):_offset(glib, "g_textBuffer") + 0x40])
        after_photo = st.read(0, size)
        f = st.files_read(SNAP + "6.bmp")
    assert after_capture == before
    assert after_photo == before   # the test's own g_textBuffer name put back; the bodies wrote nothing
    assert (f is not None) == with_dir


def _hash_tree(root):
    import hashlib
    out = {}
    for p in sorted(root.rglob("*")):
        rel = p.relative_to(root).as_posix()
        out[rel] = hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else "<dir>"
    return out


def test_capture_over_an_install_leaves_it_untouched(glib, tmp_path, monkeypatch):
    """a run over an install directory that HAS a `snapshots` folder: the BMP goes to the files' write overlay (DirGameFiles
    without write-through, as the harness and the app's install view use it: the app's own writes land in its --data directory,
    port/app/saves.cpp), every install file and directory is byte-identical afterwards, and the board's load finds the BMP
    through the overlay"""
    monkeypatch.delenv("SG_FILES_WRITE_THROUGH", raising=False)
    inst = tmp_path / "install"
    (inst / "snapshots").mkdir(parents=True)
    (inst / "Data").mkdir()
    (inst / "Data" / "a.txt").write_bytes(b"data file")
    (inst / "snapshots" / "accomp1.bmp").write_bytes(b"an older file of the install")
    before = _hash_tree(inst)
    with _state(glib) as st:
        st.files_root(str(inst))
        root = _pattern_root(st, 12)
        _capture(st, 300, 300, 1)
        f = st.files_read(SNAP + "1.bmp")
        _board(st)
        _photo(st, glib, 100, 400, 1, SNAP + "1.bmp")
        drawn = _root(st)
    assert _hash_tree(inst) == before
    assert f == _save_bmp_model(root, 300, 300)          # the overlay's copy, not the install's older file
    words = _decode_model(f)
    assert np.array_equal(drawn[400 - 0xA6:400 - 0xA6 + 0xA0, 100:100 + 0xC8], words)


# ------------------------------------------------------------------------------------------------ the photo (loadBmpSurface)
def _pixels(w, h, seed):
    rnd = random.Random(seed)
    return [[(rnd.randrange(256), rnd.randrange(256), rnd.randrange(256)) for _ in range(w)] for _ in range(h)]


def _photo_cases():
    px = _pixels(0xC8, 0xA0, 5)
    small = _pixels(37, 21, 6)
    table = [(k, 255 - k, (7 * k) & 0xFF) for k in range(256)]
    idx = [[(3 * r + c) & 0xFF for c in range(0xC8)] for r in range(0xA0)]
    return {
        "bottom_up": _bmp(0xC8, 0xA0, px),
        "top_down": _bmp(0xC8, 0xA0, px, top_down=True),
        "small": _bmp(37, 21, small),
        "eight_bit": _bmp(0xC8, 0xA0, idx, bpp=8, table=table),
    }


def _drawn(lib, files, events, mode=0):
    """the board, then one photo per (x, y, name) of `events`, over the seed-77 root picture; the root after it (the board
    image itself is not in a unit state's files: its loadImage fails and leaves the root as it was)"""
    with _Negctl(lib, mode), _state(lib) as st:
        for n, b in files.items():
            st.files_put(n, b)
        _pattern_root(st, 77)
        _board(st)
        for k, (x, y, name) in enumerate(events):
            _photo(st, lib, x, y, k + 1, name)
        return _root(st)


def _expected(before, words, x, y):
    out = before.copy()
    h, w = words.shape
    for r in range(min(h, 0xA0)):
        for c in range(min(w, 0xC8)):
            dx, dy = x + c, y - 0xA6 + r
            if 0 <= dx < 800 and 0 <= dy < 600:
                out[dy, dx] = words[r, c]
    return out


# the pin (Surface_stretchBlit of the art cut) draws nothing here: a unit state has no loadInterfaceArt cuts
@pytest.mark.parametrize("case", ["bottom_up", "top_down", "small", "eight_bit"])
def test_photo_equals_the_load_bmp_surface_model(glib, case):
    bmp = _photo_cases()[case]
    blank = _drawn(glib, {}, [])
    got = _drawn(glib, {SNAP + "5.bmp": bmp}, [(100, 400, SNAP + "5.bmp")])
    assert np.array_equal(got, _expected(blank, _decode_model(bmp), 100, 400))


def test_capture_loaded_back_is_the_root_rectangle(glib):
    with _state(glib) as st:
        st.files_put("snapshots/readme.txt", b"x")
        root = _pattern_root(st, 31)
        _capture(st, 333, 222, 8)
        f = st.files_read(SNAP + "8.bmp")
    words = _decode_model(f)
    bx, by = _clamp(333 - 0x64, 0, 0x258), _clamp(222 - 0x64, 0, 0x1B8)
    assert np.array_equal(words, root[by:by + 0xA0, bx:bx + 0xC8] & 0x7FFF)


def test_missing_file_redraws_the_previous_photo(glib):
    bmp = _photo_cases()["bottom_up"]
    blank = _drawn(glib, {}, [])
    got = _drawn(glib, {SNAP + "1.bmp": bmp}, [(20, 300, SNAP + "1.bmp"), (420, 500, SNAP + "2.bmp")])
    words = _decode_model(bmp)
    assert np.array_equal(got, _expected(_expected(blank, words, 20, 300), words, 420, 500))


def test_no_file_and_no_surface_draws_nothing(glib):
    blank = _drawn(glib, {}, [])
    assert np.array_equal(_drawn(glib, {}, [(20, 300, SNAP + "1.bmp")]), blank)


@pytest.mark.parametrize("mode", [8, 9, 10])
def test_photo_negctl_red(glib, mode):
    bmp = _photo_cases()["bottom_up"]
    events = [(20, 300, SNAP + "1.bmp"), (420, 500, SNAP + "2.bmp")]
    assert not np.array_equal(_drawn(glib, {SNAP + "1.bmp": bmp}, events, mode=mode),
                              _drawn(glib, {SNAP + "1.bmp": bmp}, events))


# ------------------------------------------------------------------------------------------------ the ORIGINAL under Unicorn
# addresses of the original (hooks.csv names; c3ea.cpp / c3ee.cpp constants)
SAVE_BMP, LOAD_BMP_SURFACE = 0x00475580, 0x00475210
FOPEN, FREAD, FWRITE, FCLOSE = 0x004A614D, 0x004A63A1, 0x004A6268, 0x004A609F
OP_NEW, OP_DELETE, MALLOC, FREE = 0x004A57F1, 0x004A4FFC, 0x004A504F, 0x004A5007
CREATE_FULL = 0x00474DD0
DISPLAY_PTR = 0x0083AD50
CODE, OBJ, HEAP, PIX = 0x20000000, 0x20010000, 0x22000000, 0x24000000
PIX_IN_DATA = 0x300000       # loadBmpSurface's destination: inside the data image, so the run's final image holds it


class _Fake:
    """the original's surroundings: a holder / Image whose handle has the slots +0x20 (pixels), +0xd8 (row length), +0xdc
    (height); the display's slot +0xb4 (0: RGB555); the CRT file calls over one in-memory file; a bump heap"""

    def __init__(self, emu, files, pix=PIX):
        self.emu, self.files, self.out, self.pix = emu, files, {}, pix
        self.heap = HEAP
        self.open = {}
        self.slot_addr = {}
        k = 0
        for name in ("pix", "w", "h", "fmt"):
            self.slot_addr[name] = CODE + 0x10 * k
            k += 1
        self.surface = None   # (w, h)

    def stubs(self):
        e = self.emu
        e.stub_py(self.slot_addr["pix"], 0, lambda uc, a, ecx: self.pix)
        e.stub_py(self.slot_addr["w"], 0, lambda uc, a, ecx: self.surface[0] if self.surface else 0)
        e.stub_py(self.slot_addr["h"], 0, lambda uc, a, ecx: self.surface[1] if self.surface else 0)
        e.stub_py(self.slot_addr["fmt"], 0, lambda uc, a, ecx: 0)
        e.stub_py(OP_NEW, 1, self.alloc)
        e.stub_py(MALLOC, 1, self.alloc)
        e.stub_py(OP_DELETE, 1, lambda uc, a, ecx: 0)
        e.stub_py(FREE, 1, lambda uc, a, ecx: 0)
        e.stub_py(FOPEN, 2, self.fopen)
        e.stub_py(FREAD, 4, self.fread)
        e.stub_py(FWRITE, 4, self.fwrite)
        e.stub_py(FCLOSE, 1, lambda uc, a, ecx: 0)

    def alloc(self, uc, a, ecx):
        p = self.heap
        self.heap += (a[0] + 0xF) & ~0xF
        return p

    def _cstr(self, uc, p):
        s = b""
        while True:
            c = bytes(uc.mem_read(p, 1))
            if c == b"\0":
                return s.decode()
            s, p = s + c, p + 1

    def fopen(self, uc, a, ecx):
        name, mode = self._cstr(uc, a[0]), self._cstr(uc, a[1])
        self.modes = getattr(self, "modes", []) + [mode]
        if "r" in mode:
            if name not in self.files:
                return 0
            h = 0x7000 + len(self.open)
            self.open[h] = [name, 0]
            return h
        h = 0x7000 + len(self.open)
        self.open[h] = [name, 0]
        self.out[name] = b""
        return h

    def fread(self, uc, a, ecx):
        p, size, n, h = a
        name, pos = self.open[h]
        data = self.files[name][pos:pos + size * n]
        got = len(data) // size if size else 0
        uc.mem_write(p, data[:got * size])
        self.open[h][1] = pos + got * size
        return got

    def fwrite(self, uc, a, ecx):
        p, size, n, h = a
        self.out[self.open[h][0]] += bytes(uc.mem_read(p, size * n))
        return n

    def handle_block(self):
        """the handle object (vtable) and the display object (vtable), as one block at OBJ"""
        b = bytearray(0x1000)
        vt = OBJ + 0x100
        struct.pack_into("<I", b, 0, vt)
        for off, name in ((0x20, "pix"), (0xD8, "w"), (0xDC, "h")):
            struct.pack_into("<I", b, 0x100 + off, self.slot_addr[name])
        struct.pack_into("<I", b, 0x400, OBJ + 0x500)     # the display object at OBJ + 0x400, its vtable at OBJ + 0x500
        struct.pack_into("<I", b, 0x500 + 0xB4, self.slot_addr["fmt"])
        return b


def _data_image(emu):
    """the exe's initialised .data (the CRT mode strings live there) with the fake display at the display pointer"""
    from port.test import cmdemu as CE  # noqa: F401
    lo = SG.DATA_VA - 0x00400000
    img = bytearray(emu.img.data[lo:lo + 0x381174])
    img += bytes(0x381174 - len(img))
    struct.pack_into("<I", img, DISPLAY_PTR - SG.DATA_VA, OBJ + 0x400)
    return bytes(img)


def _obj_block(fake, args, holder):
    """OBJ: the handle (+0), its vtable (+0x100), the display (+0x400) and its vtable (+0x500), the arguments (+0x800), the
    holder / Image (+0xc00)"""
    b = bytearray(fake.handle_block())
    b[0x800:0x800 + len(args)] = args
    b[0xC00:0xC00 + len(holder)] = holder
    return bytes(b)


def _signed(v):
    return v - (1 << 32) if v & 0x80000000 else v


@ORACLE
@pytest.mark.parametrize("sx,sy,index", CAPTURES)
def test_capture_equals_the_original_save_bmp(glib, sx, sy, index):
    from port.test import cmdemu as CE
    root, f = _captured(glib, sx, sy, index, seed=index + 1)
    emu = CE.Emu()
    fake = _Fake(emu, {})
    fake.surface = (800, 600)
    fake.stubs()
    image = _data_image(emu)
    # the original's clamp (showBulletinBoard's two calls) over the same arguments: the rectangle saveBmp gets
    bx = _signed(emu.run(0x00467130, [sx - 0x64, 0, 0x258], image).eax)
    by = _signed(emu.run(0x00467130, [sy - 0x64, 0, 0x1B8], image).eax)
    name = f"{SNAP}{index}.bmp"
    args = bytearray(0x100)
    args[0:len(name) + 1] = name.encode() + b"\0"
    struct.pack_into("<iiii", args, 0x80, bx, by, bx + 0xC8, by + 0xA0)
    holder = struct.pack("<II", 0, OBJ)   # holder + 4 = the surface handle
    res = emu.run(SAVE_BMP, [OBJ + 0x800, OBJ + 0x880], image, ecx=OBJ + 0xC00,
                  blocks=[(CODE, b"\xCC" * 0x1000), (OBJ, _obj_block(fake, args, holder)),
                          (HEAP, bytes(0x400000)), (PIX, root.astype("<u2").tobytes())])
    assert not res.err, res.err
    assert res.eax == 0 and fake.modes == ["wb"]
    assert fake.out[name] == f


@ORACLE
@pytest.mark.parametrize("case", ["bottom_up", "top_down", "small", "eight_bit"])
def test_photo_equals_the_original_load_bmp_surface(glib, case):
    from port.test import cmdemu as CE
    bmp = _photo_cases()[case]
    name = SNAP + "5.bmp"
    emu = CE.Emu()
    fake = _Fake(emu, {name: bmp}, pix=SG.DATA_VA + PIX_IN_DATA)
    fake.stubs()

    def create_full(uc, a, ecx):   # Surface_createFull(this, w, h, 0, 1, 0, 0): the handle at this + 4, a zeroed surface
        fake.surface = (_signed(a[0]), _signed(a[1]))
        uc.mem_write(ecx + 4, struct.pack("<I", OBJ))
        uc.mem_write(fake.pix, bytes(2 * a[0] * a[1]))
        return 0
    emu.stub_py(CREATE_FULL, 6, create_full, pops=True)
    args = bytearray(0x100)
    args[0:len(name) + 1] = name.encode() + b"\0"
    res = emu.run(LOAD_BMP_SURFACE, [OBJ + 0x800], _data_image(emu), ecx=OBJ + 0xC00,
                  blocks=[(CODE, b"\xCC" * 0x1000), (OBJ, _obj_block(fake, args, bytes(8))), (HEAP, bytes(0x400000))])
    assert not res.err, res.err
    assert res.eax == 0 and fake.modes == ["rb"]
    w, h = fake.surface
    words = np.frombuffer(res.image[PIX_IN_DATA:PIX_IN_DATA + 2 * w * h], dtype="<u2").reshape(h, w)
    assert np.array_equal(words, _decode_model(bmp))
    assert np.array_equal(words, _port_words(glib, bmp))


def _port_words(lib, bmp):
    """the port's photo of `bmp` as drawn on the root (a photo drawn at (100, 400): rows from 400 - 0xa6)"""
    w, hh = struct.unpack_from("<ii", bmp, 0xE + 4)
    h = abs(hh)
    got = _drawn(lib, {SNAP + "5.bmp": bmp}, [(100, 400, SNAP + "5.bmp")])
    return got[400 - 0xA6:400 - 0xA6 + h, 100:100 + w]


@ORACLE
def test_missing_photo_file_original_returns_6():
    """loadBmpSurface of a name with no file: 6 and no Surface_createFull (the Image keeps what it had)"""
    from port.test import cmdemu as CE
    emu = CE.Emu()
    fake = _Fake(emu, {})
    fake.stubs()
    made = []
    emu.stub_py(CREATE_FULL, 6, lambda uc, a, ecx: made.append(a) or 0, pops=True)
    args = bytearray(0x100)
    args[0:12] = b"snapshots\\x\0"
    res = emu.run(LOAD_BMP_SURFACE, [OBJ + 0x800], _data_image(emu), ecx=OBJ + 0xC00,
                  blocks=[(CODE, b"\xCC" * 0x1000), (OBJ, _obj_block(fake, args, bytes(8))), (HEAP, bytes(0x400000))])
    assert not res.err, res.err
    assert res.eax == 6 and not made
