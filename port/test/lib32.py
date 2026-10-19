"""Client of the Win32 runner (phase 3.R, 2026-10-19): the SAME interface as lib.py, backed by port/test/runner32/ so port
functions run as a Win32 build with real x87 (fldcw) code, driven by the 64-bit Python harness.

    SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test        # lib.load() then returns a Lib32
    from port.test import lib32; L32 = lib32.load32()          # explicitly, next to the x64 library

Protocol and design: port/test/README.md "x87 and the Win32 backend". Frames are `u32 length, u32 op, payload` over the
runner's stdin and `u32 length, i32 status, payload` back. The GameState is a named shared memory the runner copies from
and to its DLL state around every handle op, so `State32.ptr` (the base of our view) works exactly like the x64 state's
pointer and write / read / image / clear / load_image are the unchanged lib.State methods. Mapped blocks live in the
caller's ctypes buffers; the writable ones travel with each call (and come back after it), the read-only exe sections
(a ctypes buffer the caller keeps) are uploaded once per runner.
"""
from __future__ import annotations

import ctypes as C
import itertools
import mmap
import os
import pathlib
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from port.test import build as B  # noqa: E402
from port.test import lib as L  # noqa: E402

DEFAULT_CW = 0x027F
(OP_QUIT, OP_HELLO, OP_FUNCS, OP_NEGCTL_SET, OP_NEGCTL_GET, OP_NEGCTL_RESET, OP_DEFECTS, OP_DEFECT_SET, OP_DEFECT_GET,
 OP_DEFECT_RESET, OP_EXE_LOAD, OP_EXE_LOADED, OP_EXE_CONST, OP_OBJECTS, OP_HASH_BYTES, OP_STATE_NEW, OP_STATE_FREE,
 OP_CALL, OP_HASH_CLASS, OP_RUN_FRAMES, OP_EXE_FILL, OP_FILES_PUT, OP_FILES_CLEAR, OP_FILES_ROOT, OP_FILES_SIZE,
 OP_SYNC) = range(26)
P_BASE, P_MAP, P_UNMAP = 1, 2, 3


class RunnerUnavailable(RuntimeError):
    """No Win32 toolchain / runner: tests skip with this reason instead of failing."""


class RunnerDied(RuntimeError):
    pass


def _s(x: str) -> bytes:
    b = x.encode()
    return struct.pack("<I", len(b)) + b


class _Rd:
    def __init__(self, data: bytes):
        self.d, self.o = data, 0

    def u32(self):
        v = struct.unpack_from("<I", self.d, self.o)[0]
        self.o += 4
        return v

    def i32(self):
        v = struct.unpack_from("<i", self.d, self.o)[0]
        self.o += 4
        return v

    def raw(self, n):
        v = self.d[self.o:self.o + n]
        self.o += n
        return v

    def s(self):
        return self.raw(self.u32()).decode()


def available() -> str:
    """'' when the Win32 runner can be started (built, or buildable), else why not."""
    if os.name != "nt":
        return "the Win32 backend needs Windows"
    if not B.runner32_stale():
        return ""
    if not B.toolchain_ok():
        return "no Win32 toolchain (Visual Studio 2022 Build Tools + CMake) to build port/test/runner32"
    return ""


class Runner:
    """The runner process and its framing."""

    def __init__(self, exe: pathlib.Path, dll: pathlib.Path):
        self.proc = subprocess.Popen([str(exe), str(dll)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)
        self.calls = 0
        self.ro_sent: set[int] = set()
        self.ro_keep: dict[int, object] = {}

    def _read(self, n: int) -> bytes:
        out = b""
        while len(out) < n:
            chunk = self.proc.stdout.read(n - len(out))
            if not chunk:
                raise RunnerDied(f"the Win32 runner exited (code {self.proc.poll()})")
            out += chunk
        return out

    def tx(self, op: int, payload: bytes = b"") -> tuple[int, _Rd]:
        msg = struct.pack("<I", op) + payload
        try:
            self.proc.stdin.write(struct.pack("<I", len(msg)) + msg)
            n = struct.unpack("<I", self._read(4))[0]
            body = self._read(n)
        except (BrokenPipeError, OSError) as e:
            raise RunnerDied(f"the Win32 runner exited ({e})") from e
        self.calls += 1
        return struct.unpack_from("<i", body)[0], _Rd(body[4:])

    def close(self):
        try:
            if self.proc.poll() is None:
                self.proc.stdin.write(struct.pack("<II", 4, OP_QUIT))
                self.proc.stdin.close()
                self.proc.wait(timeout=10)
        except Exception:  # noqa: BLE001
            self.proc.kill()
        for f in (self.proc.stdout,):
            try:
                f.close()
            except Exception:  # noqa: BLE001
                pass


class Lib32:
    """lib.Lib's interface over the runner."""

    def __init__(self, rebuild: bool = True):
        why = available()
        if why:
            raise RunnerUnavailable(why)
        if rebuild and B.runner32_stale():
            try:
                B.build("Win32", quiet=True)
            except RuntimeError as e:
                raise RunnerUnavailable(f"Win32 build failed: {str(e)[:300]}") from e
        if B.runner32_stale():
            raise RunnerUnavailable(f"{B.runner32_path()} is not built (port/test/build.py --arch Win32)")
        self.path = B.runner32_path()
        self.runner = Runner(self.path, B.dll_path("Win32"))
        _, r = self.runner.tx(OP_HELLO)
        self.state_size = r.u32()
        self._info = r.s()
        self.pointer_size = r.u32()
        self._objects: dict = {}
        self._funcs: dict | None = None
        self.fpcw = DEFAULT_CW
        self.dll = None                      # no ctypes DLL: nothing in the harness uses it

    # ---- registry
    def functions(self) -> dict[str, dict]:
        if self._funcs is None:
            _, r = self.runner.tx(OP_FUNCS)
            out = {}
            for _ in range(r.i32()):
                name, module, abi, ret = r.s(), r.s(), r.s(), r.s()
                out[name] = {"module": module, "abi": abi, "ret": ret, "arity": r.i32(), "fpcw": r.u32()}
            self._funcs = out
        return self._funcs

    def has(self, name: str) -> bool:
        return name in self.functions()

    def negctl(self, name: str, mode: int) -> int:
        r, _ = self.runner.tx(OP_NEGCTL_SET, _s(name) + struct.pack("<i", mode))
        if r < 0:
            raise L.SgError(r, f"negctl {name}")
        return r

    def negctl_reset(self):
        self.runner.tx(OP_NEGCTL_RESET)

    def defects(self) -> dict[str, dict]:
        _, r = self.runner.tx(OP_DEFECTS)
        out = {}
        for _ in range(r.i32()):
            name, module, ref, what = r.s(), r.s(), r.s(), r.s()
            out[name] = {"module": module, "ref": ref, "what": what, "on": r.i32()}
        return out

    def defect(self, name: str, on: bool) -> int:
        r, _ = self.runner.tx(OP_DEFECT_SET, _s(name) + struct.pack("<i", 1 if on else 0))
        if r < 0:
            raise L.SgError(r, f"defect {name}")
        return r

    def defect_reset(self):
        self.runner.tx(OP_DEFECT_RESET)

    def exe_load(self, path=None) -> str:
        code, r = self.runner.tx(OP_EXE_LOAD, _s(str(path or L.exe_path())))
        err = r.s()
        return "" if code == L.SG_OK else (err or L.ERRORS.get(code, str(code)))

    def exe_loaded(self) -> bool:
        return bool(self.runner.tx(OP_EXE_LOADED)[0])

    def exe_const(self, name: str) -> bytes:
        code, r = self.runner.tx(OP_EXE_CONST, _s(name))
        if code != L.SG_OK:
            raise L.SgError(code, f"exe_const {name}")
        return r.raw(r.u32())

    def set_fpcw(self, cw: int) -> int:
        """Win32: any word is honoured (fldcw before every call); the runner restores 0x027f after it."""
        self.fpcw = cw & 0xFFFF
        return L.SG_OK

    def build_info(self) -> str:
        return self._info

    def objects(self, cls: str | None = None) -> list[tuple[str, int, int]]:
        if cls not in self._objects:
            _, r = self.runner.tx(OP_OBJECTS, _s(cls or "") + struct.pack("<I", 1 if cls else 0))
            out = []
            for _ in range(r.i32()):
                name = r.s()
                off = r.u32()
                out.append((name, off, r.u32()))
            self._objects[cls] = out
        return self._objects[cls]

    def hash_bytes(self, data: bytes) -> int:
        _, r = self.runner.tx(OP_HASH_BYTES, struct.pack("<I", len(data)) + bytes(data))
        return struct.unpack("<Q", r.raw(8))[0]

    def state(self) -> "State32":
        return State32(self)

    def close(self):
        self.runner.close()


_COUNTER = itertools.count()


class State32(L.State):
    """lib.State over the runner: same methods; the GameState is a shared mapping, edits to the address space are queued
    and sent with the next call."""

    def __init__(self, lib: Lib32):
        self.lib = lib
        self.size = lib.state_size
        self.name = f"sgrun_{os.getpid()}_{next(_COUNTER)}"
        self._mm = mmap.mmap(-1, self.size, tagname=self.name)
        self._cbuf = (C.c_char * self.size).from_buffer(self._mm)
        self.ptr = C.addressof(self._cbuf)
        code, r = lib.runner.tx(OP_STATE_NEW, _s(self.name))
        if code != L.SG_OK:
            raise MemoryError(f"runner state_new failed ({code})")
        self.h = r.u32()
        self.ctx = self.h
        self._keep = []
        self._maps: list[tuple[int, int, bool, object]] = []     # (base, size, readonly, buffer)
        self._pend = bytearray()
        self._npend = 0
        self._base = 0x20000000
        self.last_fpu: tuple[int, int] = (DEFAULT_CW, 0)           # x87 control / tag word at the end of the last call

    def close(self):
        if self.ctx:
            self.lib.runner.tx(OP_STATE_FREE, struct.pack("<I", self.h))
            self.ctx = None
            self._keep.clear()
            self._maps.clear()
            self.ptr = 0
            del self._cbuf
            self._mm.close()

    # ---- address space (edits queued, validated here like Memory::map / set_data_base)
    def _overlaps(self, lo, n, lo2, n2):
        return lo < lo2 + n2 and lo2 < lo + n

    def set_data_base(self, base: int):
        if base + self.size > 0x100000000 or base == 0 or any(self._overlaps(base, self.size, b, n) for b, n, _, _ in self._maps):
            raise L.SgError(L.SG_E_MAP, f"set_data_base 0x{base:08x}")
        self._base = base
        self._pend += struct.pack("<II", P_BASE, base)
        self._npend += 1

    @property
    def data_base(self) -> int:
        return self._base

    def map(self, base: int, buf, readonly: bool = False):
        keep_token = not isinstance(buf, (bytes, bytearray))
        if not keep_token:
            buf = (C.c_uint8 * len(buf)).from_buffer_copy(bytes(buf))
        n = C.sizeof(buf)
        if (n == 0 or base == 0 or base + n > 0x100000000 or self._overlaps(base, n, self._base, self.size)
                or any(self._overlaps(base, n, b, m) for b, m, _, _ in self._maps)):
            raise L.SgError(L.SG_E_MAP, f"map 0x{base:08x}+0x{n:x}")
        rn = self.lib.runner
        token = id(buf) if (readonly and keep_token) else 0
        send = 1
        if token:
            rn.ro_keep[token] = buf            # the id stays unique while the runner holds this reference
            send = 0 if token in rn.ro_sent else 1
            rn.ro_sent.add(token)
        elif not readonly:
            send = 0                            # writable blocks travel with each call
        self._pend += struct.pack("<IIIIQI", P_MAP, base, n, 1 if readonly else 0, token, send)
        if send:
            self._pend += C.string_at(C.addressof(buf), n)
        self._npend += 1
        self._maps.append((base, n, readonly, buf))
        self._keep.append(buf)
        return buf

    def unmap_all(self):
        self._pend += struct.pack("<I", P_UNMAP)
        self._npend += 1
        self._maps.clear()
        self._keep.clear()

    def _hdr(self) -> bytes:
        out = struct.pack("<II", self.h, self._npend) + bytes(self._pend)
        self._pend.clear()
        self._npend = 0
        return out

    def _tx(self, op: int, payload: bytes = b"") -> tuple[int, _Rd]:
        return self.lib.runner.tx(op, self._hdr() + payload)

    # ---- calls
    def call(self, name: str, args: list[int]) -> L.SgRet:
        body = _s(name) + struct.pack("<II", self.lib.fpcw, len(args)) + b"".join(struct.pack("<I", a & 0xFFFFFFFF) for a in args)
        for _, n, ro, buf in self._maps:
            if not ro:
                body += C.string_at(C.addressof(buf), n)
        code, r = self._tx(OP_CALL, body)
        if r.d:                                       # a map error before the call has no payload
            res = L.SgRet(r.u32(), r.u32(), 0.0, 0)
            res.st0 = struct.unpack("<d", r.raw(8))[0]
            res.has_st0 = r.i32()
            self.last_fpu = (r.u32(), r.u32())
            for _, n, ro, buf in self._maps:
                if not ro:
                    C.memmove(C.addressof(buf), r.raw(n), n)
        if code != L.SG_OK:
            raise L.SgError(code, f"sg_call {name}")
        return res

    def hash_class(self, cls: str) -> list[int]:
        code, r = self._tx(OP_HASH_CLASS, _s(cls))
        if code < 0:
            raise L.SgError(code, f"hash_class {cls}")
        n = r.i32()
        return list(struct.unpack(f"<{n}Q", r.raw(8 * n)))

    def run_frames(self, trace: bytes, queued: list[dict], max_frames: int):
        nsim = len(self.lib.objects("sim"))
        body = struct.pack("<I", len(trace)) + bytes(trace) + struct.pack("<i", len(queued))
        for m in queued:
            body += struct.pack("<IIII", m["msg"], m["wp"], m["lp"], m["cls"])
        code, r = self._tx(OP_RUN_FRAMES, body + struct.pack("<i", max_frames))
        if not r.d:
            return code, 0, [], []
        n = r.i32()
        ts = list(struct.unpack(f"<{n}I", r.raw(4 * n)))
        hs = struct.unpack(f"<{n * nsim}Q", r.raw(8 * n * nsim))
        return code, n, ts, [list(hs[k * nsim:(k + 1) * nsim]) for k in range(n)]

    def fill_from_exe(self, whole_image: bool = False):
        code, _ = self._tx(OP_EXE_FILL, struct.pack("<i", 1 if whole_image else 0))
        if code != L.SG_OK:
            raise L.SgError(code, "fill_from_exe")

    def files_put(self, name: str, data: bytes):
        code, _ = self._tx(OP_FILES_PUT, _s(name) + struct.pack("<I", len(data)) + bytes(data))
        if code != L.SG_OK:
            raise L.SgError(code, f"files_put {name}")

    def files_clear(self):
        self._tx(OP_FILES_CLEAR)

    def files_root(self, directory=None):
        self._tx(OP_FILES_ROOT, struct.pack("<I", 0 if directory is None else 1) + _s(str(directory or "")))

    def files_size(self, name: str) -> int:
        _, r = self._tx(OP_FILES_SIZE, _s(name))
        return struct.unpack("<q", r.raw(8))[0]


_LIB32: Lib32 | None = None


def load32(rebuild: bool = True) -> Lib32:
    """The process-wide Win32 library (starts the runner; raises RunnerUnavailable with the reason when it cannot)."""
    global _LIB32
    if _LIB32 is None or _LIB32.runner.proc.poll() is not None:
        _LIB32 = Lib32(rebuild)
    return _LIB32
