"""ctypes binding of portsim_c.dll (port/include/portsim_c.h), phase 2.6.

    from port.test import lib
    L = lib.load()                       # builds the DLL first when a source is newer (port/test/build.py)
    with L.state() as st:                # one GameState (zero-filled) and its 32-bit address space
        st.write(off, b"...")
        r = st.call("Random::range", [va, 6])
"""
from __future__ import annotations

import ctypes as C
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from port.test import build as B  # noqa: E402

SG_OK, SG_E_NO_FUNCTION, SG_E_ARITY, SG_E_FAULT, SG_E_MAP, SG_E_UNSUPPORTED, SG_E_NOT_IMPLEMENTED, SG_E_ARG, \
    SG_E_EXCEPTION = 0, -1, -2, -3, -4, -5, -6, -7, -8
ERRORS = {SG_E_NO_FUNCTION: "no such function", SG_E_ARITY: "wrong arity", SG_E_FAULT: "fault (unmapped address)",
          SG_E_MAP: "mapping refused", SG_E_UNSUPPORTED: "unsupported", SG_E_NOT_IMPLEMENTED: "not implemented",
          SG_E_ARG: "bad argument", SG_E_EXCEPTION: "exception"}
SG_E_DIVIDE, SG_E_NO_RETURN, SG_E_NO_RESOURCE = -9, -10, -11          # phase 3.0
ERRORS.update({SG_E_DIVIDE: "divide error (#DE in the original)", SG_E_NO_RETURN: "the original never returns here",
               SG_E_NO_RESOURCE: "missing exe constants / game file"})
EXE_ENV = "SG_EXE_PATH"   # the user's golf_clean.exe for ExeConstants (default original/golf_clean.exe)


def exe_path() -> pathlib.Path:
    return pathlib.Path(os.environ.get(EXE_ENV) or ROOT / "original" / "golf_clean.exe")


class SgRet(C.Structure):
    _fields_ = [("eax", C.c_uint32), ("edx", C.c_uint32), ("st0", C.c_double), ("has_st0", C.c_int32)]


class SgFuncInfo(C.Structure):
    _fields_ = [("name", C.c_char_p), ("module", C.c_char_p), ("abi", C.c_char_p), ("ret", C.c_char_p),
                ("arity", C.c_int32), ("fpcw", C.c_uint32)]


class SgDefectInfo(C.Structure):
    _fields_ = [("name", C.c_char_p), ("module", C.c_char_p), ("ref", C.c_char_p), ("what", C.c_char_p)]


class SgMsg(C.Structure):
    _fields_ = [("msg", C.c_uint32), ("wp", C.c_uint32), ("lp", C.c_uint32), ("cls", C.c_uint32)]


class SgError(RuntimeError):
    def __init__(self, code: int, what: str):
        super().__init__(f"{what}: {ERRORS.get(code, code)}")
        self.code = code


def _sig(dll):
    P, U32, I32, SZ, U64 = C.c_void_p, C.c_uint32, C.c_int32, C.c_size_t, C.c_uint64
    for name, res, args in [
        ("sg_state_size", SZ, []), ("sg_state_new", P, []), ("sg_state_free", None, [P]),
        ("sg_state_ptr", P, [P]), ("sg_set_data_base", C.c_int, [P, U32]), ("sg_data_base", U32, [P]),
        ("sg_map", C.c_int, [P, U32, P, U32, I32]), ("sg_unmap_all", None, [P]),
        ("sg_func_count", I32, []), ("sg_func_name", C.c_char_p, [I32]),
        ("sg_func_info_get", C.c_int, [I32, C.POINTER(SgFuncInfo)]), ("sg_func_find", I32, [C.c_char_p]),
        ("sg_call", C.c_int, [P, C.c_char_p, C.POINTER(U32), I32, C.POINTER(SgRet)]),
        ("sg_set_fpcw", C.c_int, [U32]),
        ("sg_negctl_set", C.c_int, [C.c_char_p, I32]), ("sg_negctl_get", I32, [C.c_char_p]),
        ("sg_negctl_reset", None, []),
        ("sg_object_count", I32, [C.c_char_p]), ("sg_object_name", C.c_char_p, [C.c_char_p, I32]),
        ("sg_object_span", C.c_int, [C.c_char_p, I32, C.POINTER(U32), C.POINTER(U32)]),
        ("sg_hash_bytes", U64, [C.c_char_p, SZ]),
        ("sg_hash_class", I32, [P, C.c_char_p, C.POINTER(U64), I32]),
        ("sg_run_frames", C.c_int, [P, C.c_char_p, SZ, C.POINTER(SgMsg), I32, I32, C.POINTER(U32), C.POINTER(U64),
                                    C.POINTER(I32)]),
        ("sg_build_info", C.c_char_p, []),
        # phase 3.0
        ("sg_defect_count", I32, []), ("sg_defect_info_get", C.c_int, [I32, C.POINTER(SgDefectInfo)]),
        ("sg_defect_set", C.c_int, [C.c_char_p, I32]), ("sg_defect_get", I32, [C.c_char_p]),
        ("sg_defect_reset", None, []),
        ("sg_exe_load", C.c_int, [C.c_char_p, C.c_char_p, SZ]), ("sg_exe_loaded", I32, []),
        ("sg_exe_const", C.c_int, [C.c_char_p, P, U32, C.POINTER(U32)]), ("sg_exe_fill_state", C.c_int, [P, I32]),
        ("sg_files_put", C.c_int, [P, C.c_char_p, C.c_char_p, SZ]), ("sg_files_clear", None, [P]),
        ("sg_files_root", C.c_int, [P, C.c_char_p]), ("sg_files_size", C.c_int64, [P, C.c_char_p]),
    ]:
        f = getattr(dll, name)
        f.restype = res
        f.argtypes = args


class Lib:
    def __init__(self, path: pathlib.Path):
        self.path = pathlib.Path(path)
        self.dll = C.CDLL(str(self.path))
        _sig(self.dll)
        self.state_size = int(self.dll.sg_state_size())
        self._objects: dict = {}

    # ---- registry
    def functions(self) -> dict[str, dict]:
        out = {}
        for i in range(self.dll.sg_func_count()):
            fi = SgFuncInfo()
            if self.dll.sg_func_info_get(i, C.byref(fi)) == SG_OK:
                out[fi.name.decode()] = {"module": fi.module.decode(), "abi": fi.abi.decode(), "ret": fi.ret.decode(),
                                         "arity": fi.arity, "fpcw": fi.fpcw}
        return out

    def has(self, name: str) -> bool:
        return self.dll.sg_func_find(name.encode()) >= 0

    def negctl(self, name: str, mode: int) -> int:
        r = self.dll.sg_negctl_set(name.encode(), mode)
        if r < 0:
            raise SgError(r, f"negctl {name}")
        return r

    def negctl_reset(self):
        self.dll.sg_negctl_reset()

    # ---- compatibility switches (phase 3.0)
    def defects(self) -> dict[str, dict]:
        out = {}
        for i in range(self.dll.sg_defect_count()):
            d = SgDefectInfo()
            if self.dll.sg_defect_info_get(i, C.byref(d)) == SG_OK:
                out[d.name.decode()] = {"module": d.module.decode(), "ref": d.ref.decode(), "what": d.what.decode(),
                                        "on": self.dll.sg_defect_get(d.name)}
        return out

    def defect(self, name: str, on: bool) -> int:
        r = self.dll.sg_defect_set(name.encode(), 1 if on else 0)
        if r < 0:
            raise SgError(r, f"defect {name}")
        return r

    def defect_reset(self):
        self.dll.sg_defect_reset()

    # ---- exe constants (phase 3.0)
    def exe_load(self, path=None) -> str:
        """Load and verify the user's exe ($SG_EXE_PATH or original/golf_clean.exe): '' or the refusal reason."""
        err = C.create_string_buffer(512)
        r = self.dll.sg_exe_load(str(path or exe_path()).encode(), err, len(err))
        return "" if r == SG_OK else (err.value.decode(errors="replace") or ERRORS.get(r, str(r)))

    def exe_loaded(self) -> bool:
        return bool(self.dll.sg_exe_loaded())

    def exe_const(self, name: str) -> bytes:
        n = C.c_uint32(0)
        r = self.dll.sg_exe_const(name.encode(), None, 0, C.byref(n))
        if r != SG_OK:
            raise SgError(r, f"exe_const {name}")
        buf = (C.c_uint8 * max(1, n.value))()
        self.dll.sg_exe_const(name.encode(), buf, n.value, C.byref(n))
        return bytes(buf)[:n.value]

    def set_fpcw(self, cw: int) -> int:
        return self.dll.sg_set_fpcw(cw)

    def build_info(self) -> str:
        return self.dll.sg_build_info().decode()

    # ---- objects / hashes
    def objects(self, cls: str | None = None) -> list[tuple[str, int, int]]:
        if cls not in self._objects:
            c = cls.encode() if cls else None
            out = []
            for i in range(self.dll.sg_object_count(c)):
                off, size = C.c_uint32(), C.c_uint32()
                self.dll.sg_object_span(c, i, C.byref(off), C.byref(size))
                out.append((self.dll.sg_object_name(c, i).decode(), off.value, size.value))
            self._objects[cls] = out
        return self._objects[cls]

    def hash_bytes(self, data: bytes) -> int:
        return int(self.dll.sg_hash_bytes(bytes(data), len(data)))

    def state(self) -> "State":
        return State(self)


class State:
    """One sg_ctx: a zero-filled GameState in a 32-bit address space. Mapped buffers are kept alive by this object."""

    def __init__(self, lib: Lib):
        self.lib = lib
        self.ctx = lib.dll.sg_state_new()
        if not self.ctx:
            raise MemoryError("sg_state_new")
        self.ptr = lib.dll.sg_state_ptr(self.ctx)
        self.size = lib.state_size
        self._keep = []

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        if self.ctx:
            self.lib.dll.sg_state_free(self.ctx)
            self.ctx = None
            self._keep.clear()

    # ---- GameState bytes
    def clear(self):
        C.memset(self.ptr, 0, self.size)

    def write(self, off: int, data: bytes):
        if off < 0 or off + len(data) > self.size:
            raise ValueError(f"write 0x{off:x}+{len(data)} outside the GameState")
        C.memmove(self.ptr + off, bytes(data), len(data))

    def read(self, off: int, n: int) -> bytes:
        if off < 0 or off + n > self.size:
            raise ValueError(f"read 0x{off:x}+{n} outside the GameState")
        return C.string_at(self.ptr + off, n)

    def load_image(self, data: bytes):
        if len(data) != self.size:
            raise ValueError(f"image is {len(data)} bytes, GameState {self.size}")
        C.memmove(self.ptr, bytes(data), self.size)

    def image(self) -> bytes:
        return C.string_at(self.ptr, self.size)

    # ---- address space
    def set_data_base(self, base: int):
        r = self.lib.dll.sg_set_data_base(self.ctx, base)
        if r != SG_OK:
            raise SgError(r, f"set_data_base 0x{base:08x}")

    @property
    def data_base(self) -> int:
        return int(self.lib.dll.sg_data_base(self.ctx))

    def map(self, base: int, buf, readonly: bool = False):
        """Map a ctypes buffer (create_string_buffer / bytearray-backed) at virtual `base`."""
        if isinstance(buf, (bytes, bytearray)):
            buf = (C.c_uint8 * len(buf)).from_buffer_copy(bytes(buf))
        r = self.lib.dll.sg_map(self.ctx, base, C.addressof(buf), C.sizeof(buf), 1 if readonly else 0)
        if r != SG_OK:
            raise SgError(r, f"map 0x{base:08x}+0x{C.sizeof(buf):x}")
        self._keep.append(buf)
        return buf

    def unmap_all(self):
        self.lib.dll.sg_unmap_all(self.ctx)
        self._keep.clear()

    # ---- exe constants and game files (phase 3.0)
    def fill_from_exe(self, whole_image: bool = False):
        r = self.lib.dll.sg_exe_fill_state(self.ctx, 1 if whole_image else 0)
        if r != SG_OK:
            raise SgError(r, "fill_from_exe")

    def files_put(self, name: str, data: bytes):
        r = self.lib.dll.sg_files_put(self.ctx, name.encode(), bytes(data), len(data))
        if r != SG_OK:
            raise SgError(r, f"files_put {name}")

    def files_clear(self):
        self.lib.dll.sg_files_clear(self.ctx)

    def files_root(self, directory=None):
        self.lib.dll.sg_files_root(self.ctx, str(directory).encode() if directory is not None else None)

    def files_size(self, name: str) -> int:
        return int(self.lib.dll.sg_files_size(self.ctx, name.encode()))

    # ---- calls and hashes
    def call(self, name: str, args: list[int]) -> SgRet:
        arr = (C.c_uint32 * max(1, len(args)))(*[a & 0xFFFFFFFF for a in args])
        r = SgRet()
        code = self.lib.dll.sg_call(self.ctx, name.encode(), arr, len(args), C.byref(r))
        if code != SG_OK:
            raise SgError(code, f"sg_call {name}")
        return r

    def hash_class(self, cls: str) -> list[int]:
        n = self.lib.dll.sg_object_count(cls.encode())
        out = (C.c_uint64 * max(1, n))()
        got = self.lib.dll.sg_hash_class(self.ctx, cls.encode(), out, n)
        if got < 0:
            raise SgError(got, f"hash_class {cls}")
        return list(out[:got])

    def run_frames(self, trace: bytes, queued: list[dict], max_frames: int):
        """sg_run_frames: (code, frames_done, [T], [[hash] * n_sim])."""
        nsim = self.lib.dll.sg_object_count(b"sim")
        q = (SgMsg * max(1, len(queued)))(*[SgMsg(m["msg"], m["wp"], m["lp"], m["cls"]) for m in queued])
        out_t = (C.c_uint32 * max(1, max_frames))()
        out_h = (C.c_uint64 * max(1, max_frames * nsim))()
        done = C.c_int32(0)
        code = self.lib.dll.sg_run_frames(self.ctx, bytes(trace), len(trace), q, len(queued), max_frames, out_t, out_h,
                                          C.byref(done))
        n = done.value
        return code, n, list(out_t[:n]), [list(out_h[k * nsim:(k + 1) * nsim]) for k in range(n)]


_LIB: Lib | None = None


def load(path: str | os.PathLike | None = None, rebuild: bool = True) -> Lib:
    """The DLL at `path`, $SG_PORTSIM_DLL, or port/build/<arch>/Release/portsim_c.dll (built first when stale)."""
    global _LIB
    if path is None and _LIB is not None:
        return _LIB
    p = path or os.environ.get("SG_PORTSIM_DLL")
    if p is None:
        if rebuild and B.stale():
            B.build(quiet=True)
        p = B.dll_path()
    lib = Lib(pathlib.Path(p))
    if path is None:
        _LIB = lib
    return lib
