"""Emulator oracle helpers of phase 3.8 stream 38b (development only, opt-in with SG_EMU_ORACLE=1).

The ORIGINAL golf_clean.exe runs under Unicorn (port/test/cmdemu.py) with its file layer stood in by a Python dict:
  * the static CRT's fopen / fread / fgets / fclose / strtok and the low-level _open / _read / _write / _close / _tell
    (addresses below) serve and record bytes of `Files`, a case-insensitive {path: bytes} dictionary;
  * KERNEL32 FindFirstFileA / FindNextFileA / FindClose serve listings computed by the REAL Windows FindFirstFileA on a
    directory made from the same dictionary (`WinListing`): the listing order and the wildcard rules are the host's, not a
    model of ours.
Drawing and window callees are dropped by their own `ret N` (Emu.drop). Everything else runs as the original's code.
"""
from __future__ import annotations

import ctypes
import os
import pathlib
import shutil
import struct
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from port.test import cmdemu as CE  # noqa: E402  (puts re/tools on sys.path)
import vector_lib as VL  # noqa: E402

DATA_VA = VL.DATA_VA
# the static CRT's functions (golf_clean.exe)
A_FOPEN, A_FREAD, A_FGETS, A_FCLOSE, A_STRTOK = 0x004A614D, 0x004A63A1, 0x004A65EE, 0x004A609F, 0x004A678B
A_FWRITE = 0x004A6268


def norm(path: str) -> str:
    p = path.replace("\\", "/")
    while p.startswith("./"):
        p = p[2:]
    return p.lstrip("/").lower()


class Files:
    """{normalised path: bytes} plus the original's spelling of each path (for real directories)"""

    def __init__(self, items=None):
        self.data: dict[str, bytes] = {}
        self.spell: dict[str, str] = {}
        for k, v in (items or {}).items():
            self.put(k, v)

    def put(self, name: str, data: bytes):
        self.data[norm(name)] = bytes(data)
        self.spell[norm(name)] = name.replace("\\", "/")

    def get(self, name: str):
        return self.data.get(norm(name))

    def copy(self):
        f = Files()
        f.data, f.spell = dict(self.data), dict(self.spell)
        return f


def load_install_themes(install: pathlib.Path) -> Files:
    """every file below Themes\\ of the user's install"""
    f = Files()
    base = install / "Themes"
    for p in sorted(base.rglob("*")):
        if p.is_file():
            f.put("Themes/" + p.relative_to(base).as_posix(), p.read_bytes())
    return f


class WinListing:
    """Windows' own FindFirstFileA over a real directory made from a Files dictionary."""

    def __init__(self, files: Files):
        self.root = pathlib.Path(tempfile.mkdtemp(prefix="sg38b_"))
        for k, v in files.data.items():
            p = self.root / files.spell[k]
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(v)
        self._k32 = ctypes.WinDLL("kernel32", use_last_error=True)

    def close(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def find(self, pattern: str) -> list[tuple[str, bool]]:
        """[(cFileName, is_directory)] in FindNextFileA order; [] when FindFirstFileA fails"""
        class FD(ctypes.Structure):
            _pack_ = 4
            _fields_ = [("attr", ctypes.c_uint32), ("ct", ctypes.c_uint64), ("at", ctypes.c_uint64),
                        ("wt", ctypes.c_uint64), ("hi", ctypes.c_uint32), ("lo", ctypes.c_uint32),
                        ("r0", ctypes.c_uint32), ("r1", ctypes.c_uint32), ("name", ctypes.c_char * 260),
                        ("alt", ctypes.c_char * 14)]
        k = self._k32
        k.FindFirstFileA.restype = ctypes.c_void_p
        k.FindFirstFileA.argtypes = [ctypes.c_char_p, ctypes.POINTER(FD)]
        k.FindNextFileA.argtypes = [ctypes.c_void_p, ctypes.POINTER(FD)]
        k.FindClose.argtypes = [ctypes.c_void_p]
        fd = FD()
        full = str(self.root / pattern.replace("\\", "/")).encode("mbcs")
        h = k.FindFirstFileA(full, ctypes.byref(fd))
        if h in (None, ctypes.c_void_p(-1).value, 0xFFFFFFFFFFFFFFFF):
            return []
        out = []
        while True:
            out.append((fd.name.decode("latin-1"), bool(fd.attr & 0x10)))
            if not k.FindNextFileA(ctypes.c_void_p(h), ctypes.byref(fd)):
                break
        k.FindClose(ctypes.c_void_p(h))
        return out


def cstr(uc, p, n=4096) -> bytes:
    out = b""
    while len(out) < n:
        c = bytes(uc.mem_read(p + len(out), 1))
        if c == b"\0":
            break
        out += c
    return out


class CrtModel:
    """The file layer served to one emulator run; `log` records every call (name, argument summary)."""

    def __init__(self, files: Files, listing: WinListing | None = None, live: bool = False):
        self.files = files
        self.listing = listing
        self.live = live
        self.log: list = []
        self.handles: dict[int, dict] = {}
        self.next_handle = 0x50000000
        self.find: dict[int, list] = {}
        self.next_find = 0x61000000
        self.writes: dict[str, bytes] = {}   # files the run wrote (stdio)

    # ---- stdio (static CRT)
    def fopen(self, uc, a, ecx):
        path = cstr(uc, a[0]).decode("latin-1")
        mode = cstr(uc, a[1]).decode("latin-1")
        self.log.append(("fopen", path, mode))
        data = self.files.get(path)
        if "w" in mode:
            h = self.next_handle
            self.next_handle += 0x20
            self.handles[h] = {"data": bytearray(), "pos": 0, "path": norm(path), "write": True, "spell": path}
            return h
        if data is None:
            return 0
        h = self.next_handle
        self.next_handle += 0x20
        text = "b" not in mode
        if text:   # text mode: CR LF -> LF, Ctrl-Z ends the data
            out = bytearray()
            i = 0
            while i < len(data):
                if data[i] == 0x1A:
                    break
                if data[i] == 0x0D and i + 1 < len(data) and data[i + 1] == 0x0A:
                    i += 1
                    continue
                out.append(data[i])
                i += 1
            data = bytes(out)
        self.handles[h] = {"data": data, "pos": 0, "path": norm(path)}
        return h

    def fread(self, uc, a, ecx):
        buf, size, count, f = a
        h = self.handles[f]
        self.log.append(("fread", h["path"], size, count))
        want = size * count
        chunk = h["data"][h["pos"]:h["pos"] + want]
        h["pos"] += len(chunk)
        uc.mem_write(buf, chunk)
        return len(chunk) // size if size else 0

    def fgets(self, uc, a, ecx):
        buf, n, f = a
        h = self.handles[f]
        data, pos = h["data"], h["pos"]
        if pos >= len(data):
            return 0
        out = bytearray()
        while len(out) < n - 1 and pos < len(data):
            c = data[pos]
            pos += 1
            out.append(c)
            if c == 0x0A:
                break
        h["pos"] = pos
        uc.mem_write(buf, bytes(out) + b"\0")
        return buf

    def fwrite(self, uc, a, ecx):
        buf, size, count, f = a
        h = self.handles[f]
        data = bytes(uc.mem_read(buf, size * count))
        h["data"] += data
        self.log.append(("fwrite", h["path"], size, count))
        return count

    def fclose(self, uc, a, ecx):
        h = self.handles[a[0]]
        self.log.append(("fclose", h["path"]))
        if h.get("write"):
            self.writes[h["path"]] = bytes(h["data"])
            self.files.put(h["spell"], bytes(h["data"]))
        del self.handles[a[0]]
        return 0

    _tok = None

    def strtok(self, uc, a, ecx):
        s, delim = a
        d = cstr(uc, delim)
        if s != 0:
            self._tok = s
        p = self._tok
        if p is None or p == 0:
            return 0
        while True:   # skip leading delimiters
            c = bytes(uc.mem_read(p, 1))
            if c == b"\0":
                self._tok = None
                return 0
            if c in [bytes([x]) for x in d]:
                p += 1
                continue
            break
        start = p
        while True:
            c = bytes(uc.mem_read(p, 1))
            if c == b"\0":
                self._tok = None
                return start
            if c in [bytes([x]) for x in d]:
                uc.mem_write(p, b"\0")
                self._tok = p + 1
                return start
            p += 1

    # ---- KERNEL32 find
    def find_first(self, uc, a):
        pattern = cstr(uc, a[0]).decode("latin-1")
        self.log.append(("FindFirstFileA", pattern))
        if self.live:   # the files change during a run (saves written, files deleted): list the current set
            lst = WinListing(self.files)
            try:
                entries = lst.find(pattern)
            finally:
                lst.close()
        else:
            entries = self.listing.find(pattern) if self.listing else []
        if not entries:
            return 0xFFFFFFFF
        h = self.next_find
        self.next_find += 1
        self.find[h] = entries
        self._fill(uc, a[1], self.find[h].pop(0))
        return h

    def find_next(self, uc, a):
        h = a[0]
        if not self.find.get(h):
            return 0
        self._fill(uc, a[1], self.find[h].pop(0))
        return 1

    def find_close(self, uc, a):
        self.find.pop(a[0], None)
        return 1

    @staticmethod
    def _fill(uc, ptr, entry):
        name, is_dir = entry
        uc.mem_write(ptr, struct.pack("<I", 0x10 if is_dir else 0x80))
        uc.mem_write(ptr + 0x2C, name.encode("latin-1") + b"\0")


def make_emu(model: CrtModel, drops=(), recorded=(), emu=None) -> CE.Emu:
    emu = emu or CE.Emu()
    emu.stub_py(A_FOPEN, 2, model.fopen, name="fopen")
    emu.stub_py(A_FREAD, 4, model.fread, name="fread")
    emu.stub_py(A_FGETS, 3, model.fgets, name="fgets")
    emu.stub_py(A_FCLOSE, 1, model.fclose, name="fclose")
    emu.stub_py(A_FWRITE, 4, model.fwrite, name="fwrite")
    emu.stub_py(A_STRTOK, 2, model.strtok, name="strtok")
    emu.import_handlers["FindFirstFileA"] = (2, model.find_first)
    emu.import_handlers["FindNextFileA"] = (2, model.find_next)
    emu.import_handlers["FindClose"] = (1, model.find_close)
    for a in drops:
        emu.drop(a)
    for a, name in recorded:   # a callee recorded (name, arguments) and answered 0
        emu.drop(a, 0, name)
    return emu


def have_oracle() -> bool:
    return os.environ.get("SG_EMU_ORACLE") in ("1", "update")


# ---- the pinned digests of the oracle's results (port/test/golden_38b.json): one section per test module
import hashlib  # noqa: E402
import json  # noqa: E402

GOLDEN = ROOT / "port" / "test" / "golden_38b.json"


def golden_load() -> dict:
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


def golden_update(section: str, data) -> None:
    g = golden_load()
    g[section] = data
    GOLDEN.write_text(json.dumps(g, indent=0, sort_keys=True))


def files_sha(files: "Files") -> str:
    h = hashlib.sha256()
    for k in sorted(files.data):
        h.update(k.encode())
        h.update(hashlib.sha256(files.data[k]).digest())
    return h.hexdigest()[:16]


# ---- low-level CRT (_open / _read / _write / _close / _tell) and the peers the save / load functions record
A_OPEN, A_READ, A_WRITE, A_CLOSE, A_TELL = 0x004A5D48, 0x004A583A, 0x004A5B58, 0x004A5A78, 0x004A6057
A_REMOVE = 0x004A64B8
# (address, hooks.csv name, stack arguments, thiscall): callees of loadGame / loadCourse / saveGame the port reaches through
# the registry; the original's are stubbed and logged, the port's recorded (commands.recordPeer), and the call lists compared
SAVE_PEERS = [
    (0x0042F7A0, "rebuildHeightfield", 0, False), (0x0042F340, "rebuildTileCoverage", 0, False),
    (0x0042F2C0, "markTileKind11", 0, False), (0x00449400, "applyCourseType", 1, False),
    (0x0043DBE0, "loadWorldAssets", 0, False), (0x00449520, "refreshTerrain", 0, False),
    (0x0040D320, "drawWindow", 4, False), (0x00480C80, "Window::refreshFull", 1, True),
    (0x00442180, "loadInterfaceArt", 1, False), (0x004315E0, "buildGolferBioText", 0, False),
    (0x00483D30, "enterModal483d30", 0, False), (0x0046D6E0, "showPopup", 5, False),
    (0x00449540, "rebuildTerrainGrid", 0, False),
    (0x0046DE70, "scrollTextWindow", 3, False), (0x004481B0, "playSound", 5, False), (0x0040CB00, "startMessage", 3, False), (0x0045B2C0, "textInputBox", 4, False),
]


def fnv1a(s: str) -> int:
    h = 2166136261
    for ch in s.encode():
        h = ((h ^ ch) * 16777619) & 0xFFFFFFFF
    return h


class LowIo:
    """_open / _read / _write / _close / _tell over a Files dictionary (binary files; descriptor 4, the first the CRT hands out)"""

    def __init__(self, files: Files):
        self.files = files
        self.fd = None
        self.written: dict[str, bytes] = {}
        self.removed: list = []
        self.log: list = []
        self.peer_calls: list = []   # (fnv1a(name), args...) as the port's recordPeer logs them

    def open(self, uc, a, ecx):
        path = cstr(uc, a[0]).decode("latin-1")
        flags = a[1]
        self.log.append(("_open", path, flags))
        write = bool(flags & 0x300)
        if write:
            self.fd = {"path": norm(path), "data": bytearray(), "pos": 0, "write": True, "spell": path}
            return 4
        data = self.files.get(path)
        if data is None:
            return 0xFFFFFFFF
        self.fd = {"path": norm(path), "data": data, "pos": 0, "write": False}
        return 4

    def read(self, uc, a, ecx):
        fd, buf, n = a
        f = self.fd
        if fd != 4 or f is None or f["write"]:
            return 0xFFFFFFFF
        chunk = f["data"][f["pos"]:f["pos"] + n]
        f["pos"] += len(chunk)
        uc.mem_write(buf, bytes(chunk))
        return len(chunk)

    def write(self, uc, a, ecx):
        fd, buf, n = a
        f = self.fd
        if fd != 4 or f is None or not f["write"]:
            return 0xFFFFFFFF
        f["data"] += bytes(uc.mem_read(buf, n))
        return n

    def close(self, uc, a, ecx):
        f = self.fd
        if a[0] != 4 or f is None:
            return 0xFFFFFFFF
        if f["write"]:
            self.written[f["path"]] = bytes(f["data"])
            self.files.put(f["spell"], bytes(f["data"]))   # later FindFirstFileA calls list it
        self.fd = None
        return 0

    def remove(self, uc, a, ecx):
        path = cstr(uc, a[0]).decode("latin-1")
        self.log.append(("_remove", path))
        if self.files.get(path) is None:
            return 0xFFFFFFFF
        self.files.data.pop(norm(path), None)
        self.files.spell.pop(norm(path), None)
        self.removed.append(norm(path))
        return 0

    def tell(self, uc, a, ecx):
        return self.fd["pos"] if self.fd else 0xFFFFFFFF


def add_save_stubs(emu: CE.Emu, io: LowIo, peers=None) -> CE.Emu:
    """the low-level CRT and the recorded peers (default SAVE_PEERS) on `emu`"""
    emu.stub_py(A_OPEN, 3, io.open, name="_open")
    emu.stub_py(A_READ, 3, io.read, name="_read")
    emu.stub_py(A_WRITE, 3, io.write, name="_write")
    emu.stub_py(A_CLOSE, 1, io.close, name="_close")
    emu.stub_py(A_TELL, 1, io.tell, name="_tell")
    emu.stub_py(A_REMOVE, 1, io.remove, name="_remove")
    for addr, name, nargs, thiscall in (SAVE_PEERS if peers is None else peers):
        def fn(uc, a, ecx, _n=name, _t=thiscall):
            args = ([ecx] if _t else []) + list(a)
            io.peer_calls.append(tuple([fnv1a(_n)] + args + [0] * (6 - len(args)) + [len(args)]))
            return 0
        emu.stub_py(addr, nargs, fn, pops=thiscall, name=name)
    return emu


def make_save_emu(io: LowIo, extra_stubs=()) -> CE.Emu:
    return add_save_stubs(CE.Emu(), io)
