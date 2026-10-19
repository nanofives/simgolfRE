"""LiveRunner: judges a port function against the LIVE observations of the raw vector captures (phase 3.5).

The vector corpus (port/corpus/vectors/<function>.vec, re/analysis/data/vector_corpus.md) keeps only the vectors the Unicorn
emulation of the original could reproduce. The raw captures (port/corpus/vectors/raw/<key>.raw.gz) hold every vector the
fixtures ran in the live game, with the live return value and the live byte changes of the whole data section. A function the
emulation cannot run (story files through the CRT's locked stdio, long builders with stood-in callees) has no .vec at all,
but its raw capture is a valid oracle: this runner starts the port from the captured data image (the fixture's drift
applied), maps the captured blocks at their live addresses, calls the function and compares

  * the return value with the live one,
  * every byte the port changed (data section and blocks) with the live post-state,
  * every byte the live run changed in a sim-class object with the port's post-state.

Bytes of pointer, audio and unmodelled (gap) objects are never compared (re/analysis/data/gamestate_gen.md); a live change
the port never made in another class is counted as `noise` (the menu thread runs while a fixture does) and reported.
The raw captures are local files (gitignored): without them nothing runs. No game, Frida or original binary is involved.
"""
from __future__ import annotations

import ctypes as C
import dataclasses
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
for p in (ROOT, ROOT / "re" / "tools"):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))
import vector_capture as VC  # noqa: E402
import vector_lib as VL  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test.vectors import exe_sections  # noqa: E402

RAW_DIR = VL.VEC_DIR / "raw"
STRICT_CLASSES = ("sim", "hidden")      # a live change here the port did not make is a failure, not noise


@dataclasses.dataclass
class LiveResult:
    key: str
    function: str
    run: int = 0
    passed: int = 0
    skipped: dict = dataclasses.field(default_factory=dict)
    noise: int = 0                     # vectors with a live change outside the sim classes that the port did not make
    failures: list = dataclasses.field(default_factory=list)
    distinct: int = 0

    @property
    def verdict(self) -> str:
        return "NO_VECTORS" if self.run == 0 else ("GREEN" if not self.failures else "RED")

    def summary(self) -> str:
        sk = ", ".join(f"{k} {v}" for k, v in sorted(self.skipped.items())) or "none"
        s = f"{self.key}: {self.verdict} {self.passed}/{self.run} passed, skipped {sk}, noise {self.noise}, distinct {self.distinct}"
        if self.failures:
            s += f"; first failure vector {self.failures[0][0]}: {self.failures[0][1]}"
        return s


def raw_path(key: str) -> pathlib.Path:
    return RAW_DIR / f"{key.replace('::', '_')}.raw.gz"


class LiveRunner:
    def __init__(self, lib: L.Lib | None = None):
        self.lib = lib or L.load()
        self.gs = VL.GsMap()
        n = VL.DATA_SIZE
        self.excluded = np.zeros(n, bool)
        self.strict = np.zeros(n, bool)
        for off, size, sym, cls, ptr in self.gs.items:
            if cls in ("pointer", "audio", "gap"):
                self.excluded[off:off + size] = True
            elif cls in STRICT_CLASSES:
                self.strict[off:off + size] = True

    def _sym(self, off: int) -> str:
        it = self.gs.at(off)
        return f"{it[2]}+0x{off - it[0]:x}"

    def run_key(self, function: str, key: str, max_vectors: int | None = None, only: set | None = None, dump: str | None = None,
                ignore: tuple = ()) -> LiveResult:
        res = LiveResult(key, function)
        path = raw_path(key)
        if not path.exists():
            res.skipped["no raw capture"] = 1
            return res
        meta, blob = VC.read_raw(path)
        if meta["status"] != "ok":
            res.skipped[f"capture {meta['status']}"] = 1
            return res
        kc = VC.KeyCapture(meta, blob)
        types = meta["args"]
        rets = meta["ret"]
        data = bytearray(kc.image)
        distinct = set()
        excluded = self.excluded.copy()
        for name in ignore:                      # objects a stood-in callee of the capture would have written (per key)
            it = self.gs.by_sym[name]
            excluded[it[0]:it[0] + it[1]] = True
        with self.lib.state() as st:
            st.set_data_base(VL.DATA_VA)
            if (ROOT / "original").is_dir():
                st.files_root(ROOT / "original")
            for name, va, d in exe_sections():
                lo, hi = va, va + len(d)
                if lo < VL.DATA_VA + VL.DATA_SIZE and VL.DATA_VA < hi:
                    continue
                st.map(va, d, readonly=True)
            bufs = []
            for base, b, role in kc.blocks:
                buf = (C.c_uint8 * max(1, len(b))).from_buffer_copy(bytes(b).ljust(max(1, len(b)), b"\0"))
                try:
                    st.map(base, buf)
                except L.SgError:
                    buf = None
                bufs.append(buf)
            for row in meta["rows"]:
                VC._apply_dw(data, row["drift"])
                if only is not None and row["i"] not in only:
                    continue
                if max_vectors is not None and res.run >= max_vectors:
                    break
                if row["err"]:
                    res.skipped["live_error"] = res.skipped.get("live_error", 0) + 1
                    continue
                st.load_image(bytes(data))
                for (base, b, role), buf in zip(kc.blocks, bufs):
                    if buf is not None and len(b):
                        C.memmove(buf, bytes(b), len(b))
                args = []
                for t, v in zip(types, VC._arg_values(kc, row, types, None)):
                    if t in ("double",):
                        import struct
                        lo, hi = struct.unpack("<II", struct.pack("<d", v))
                        args += [lo, hi]
                    else:
                        args.append(int(v) & 0xFFFFFFFF)
                res.run += 1
                why = None
                try:
                    r = st.call(function, args)
                except L.SgError as e:
                    r = None
                    why = f"call: {e}"
                post = bytearray(data)
                VC._apply_dw(post, row["dataDiff"])
                got = np.frombuffer(st.image(), np.uint8)
                pre = np.frombuffer(bytes(data), np.uint8)
                exp = np.frombuffer(bytes(post), np.uint8)
                if why is None and rets != "void":
                    live = VC.live_ret(meta, row)
                    mine = VL.norm_ret(rets, r.eax, r.edx)
                    if not VL.float_eq(live, mine):
                        why = f"ret {mine!r} != live {live!r}"
                if why is None:
                    bad = np.nonzero((got != pre) & (got != exp) & ~excluded)[0]
                    if len(bad) and dump:
                        for o in bad[:40]:
                            print("DIFF", self._sym(int(o)), "port", hex(int(got[o])), "live", hex(int(exp[o])), "pre", hex(int(pre[o])))
                    if len(bad):
                        o = int(bad[0])
                        why = (f"{self._sym(o)}: port 0x{int(got[o]):02x}, live 0x{int(exp[o]):02x} "
                               f"(pre 0x{int(pre[o]):02x}; {len(bad)} bytes differ)")
                if why is None:
                    miss = np.nonzero((exp != pre) & (got == pre) & self.strict & ~excluded)[0]
                    if len(miss):
                        o = int(miss[0])
                        why = f"{self._sym(o)}: live wrote 0x{int(exp[o]):02x}, the port left 0x{int(pre[o]):02x} ({len(miss)} bytes)"
                if why is None:
                    noise = np.nonzero((exp != pre) & (got == pre) & ~self.strict & ~excluded)[0]
                    if len(noise):
                        res.noise += 1
                # blocks: the port's changes must equal the live ones
                if why is None:
                    for bi, ((base, b, role), buf) in enumerate(zip(kc.blocks, bufs)):
                        if buf is None or not len(b):
                            continue
                        now = bytes(buf)[:len(b)]
                        if now == bytes(b):
                            continue
                        exp_b = bytearray(b)
                        if bi < len(row["blockDiff"]):
                            d = row["blockDiff"][bi]
                            for k in range(0, len(d), 2):
                                exp_b[d[k]] = d[k + 1]
                        k = next((i for i in range(len(b)) if now[i] != b[i] and now[i] != exp_b[i]), None)
                        if k is not None:
                            why = f"block {bi}+0x{k:x}: port 0x{now[k]:02x}, live 0x{exp_b[k]:02x} (pre 0x{b[k]:02x})"
                            break
                if dump:
                    o = [x for x in self.gs.items if x[2] == dump][0][0]
                    print("PORT", bytes(got[o:o+0x80]))
                    print("LIVE", bytes(exp[o:o+0x80]))
                    print("PRE ", bytes(pre[o:o+0x80]))
                distinct.add((r.eax if r is not None else None, hash(bytes(got[~excluded][::97]))))
                if why:
                    res.failures.append((row["i"], why))
                else:
                    res.passed += 1
            st.unmap_all()
        res.distinct = len(distinct)
        return res


def keys_of(function: str) -> list[str]:
    """Raw capture keys of a function (the key names of its registry entries, plus their +unstub captures)."""
    if not RAW_DIR.exists():
        return []
    out = []
    for p in sorted(RAW_DIR.glob(f"{function}*.raw.gz")):
        stem = p.name[:-len(".raw.gz")]
        if stem == function or stem.startswith(function + "_") or stem.startswith(function + "+"):
            out.append(stem)
    return out
