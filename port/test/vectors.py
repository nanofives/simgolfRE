"""VectorRunner: runs a port function over the recorded vectors of the original (phase 2.6).

Corpus: port/corpus/vectors/<function>.vec (schema re/model/vector_schema.md, reader re/tools/vector_lib.py). Rules from
re/analysis/data/vector_corpus.md section 8 and re/PHASE23_PLAN.md section 8.7:

1. Only `status: ok` vectors are tests; a vector with `reloc_check: false` is skipped (reason recorded).
2. GameState zero-filled, the key's `seed` then the vector's `in` written; every block allocated, copied and mapped
   in the context's 32-bit address space (at bases whose page offsets differ from the capture's), `relocs` applied.
3. The x87 control word set to the key's `fpcw` when the key is `x87` or the function declares one.
4. The function called by name with `args` (a `ptr` location resolved to this address space).
5. `ret` compared (pointers and `ret_loc` as locations, NaN / inf through `float_repr`), every `out` region compared
   (each `out_ptrs` dword as the address of its `to` location), and no byte may change outside the `out` regions
   (GameState and blocks; the strict written-set check of vector_validate.py emu, minus same-value writes, which a
   memory diff cannot see).

The GameState is mapped at the original data section's base (DATA_VA, tooling knowledge kept in this harness): state
objects of class `pointer` hold the original's addresses (U-0703), and a port function that follows one then lands on the
same object. Sections of the user's own golf_clean.exe (original/) other than .data are mapped read-only at their image
addresses when the file is present (constants and string literals a pointer in state refers to).
"""
from __future__ import annotations

import collections
import csv
import ctypes as C
import dataclasses
import json
import pathlib
import struct
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
for p in (ROOT, ROOT / "re" / "tools"):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))
import vector_lib as VL  # noqa: E402
from port.test import lib as L  # noqa: E402

COVERAGE_TSV = ROOT / "re" / "port" / "vectors_coverage.tsv"
MANIFEST_TSV = ROOT / "re" / "port" / "vectors_manifest.tsv"
BLOCK_BASE = 0x40000000
MIN_VECTORS = 10          # the C3 leaf discipline (>= 10 vectors)


def vec_path(function: str) -> pathlib.Path:
    return VL.VEC_DIR / f"{function.replace('::', '_')}.vec"


def coverage_table() -> dict[str, dict]:
    if not COVERAGE_TSV.exists():
        return {}
    with COVERAGE_TSV.open(newline="") as f:
        return {r["function"]: r for r in csv.DictReader(f, delimiter="\t")}


def block_bases(sizes: list[int]) -> list[int]:
    """Packed bases from BLOCK_BASE, a guard page between blocks, page offset 0x10 * (i + 1) (never the capture's)."""
    out, cur = [], BLOCK_BASE
    for i, n in enumerate(sizes):
        b = cur + ((0x10 * (i + 1)) & 0xFFF)
        out.append(b)
        cur = (b + max(n, 1) + 0xFFF) // 0x1000 * 0x1000 + 0x1000
    return out


_EXE = None


def exe_sections():
    """[(name, va, ctypes buffer)] of the user's own golf_clean.exe, or [] when it is not there."""
    global _EXE
    if _EXE is None:
        _EXE = []
        if VL.EXE.exists():
            import pefile
            pe = pefile.PE(str(VL.EXE), fast_load=True)
            for s in pe.sections:
                name = s.Name.rstrip(b"\0").decode(errors="replace")
                va = pe.OPTIONAL_HEADER.ImageBase + s.VirtualAddress
                size = max(s.Misc_VirtualSize, s.SizeOfRawData)
                data = s.get_data()[:size].ljust(size, b"\0")
                _EXE.append((name, va, (C.c_uint8 * size).from_buffer_copy(data)))   # mapped read-only, copied once
            pe.close()
    return _EXE


@dataclasses.dataclass
class VectorFailure:
    key: str
    i: int
    why: str


@dataclasses.dataclass
class FunctionResult:
    function: str
    implemented: bool
    vec_file: bool = False
    keys: int = 0
    total: int = 0                   # every vector in the file
    ok: int = 0                      # status ok
    run: int = 0
    passed: int = 0
    failed: int = 0
    skipped: collections.Counter = dataclasses.field(default_factory=collections.Counter)
    distinct: int = 0                # distinct expected (ret, out) pairs over the vectors run
    distinct_corpus: int | None = None
    coverage: str = "-"
    warnings: list = dataclasses.field(default_factory=list)
    failures: list = dataclasses.field(default_factory=list)

    @property
    def verdict(self) -> str:
        if not self.implemented:
            return "NOT_PORTED"
        if not self.vec_file or self.run == 0:
            return "NO_VECTORS"
        return "GREEN" if self.failed == 0 else "RED"

    def summary(self) -> str:
        sk = ", ".join(f"{k} {v}" for k, v in sorted(self.skipped.items())) or "none"
        s = (f"{self.function}: {self.verdict} {self.passed}/{self.run} passed, skipped {sk}; distinct {self.distinct}; "
             f"coverage {self.coverage}")
        if self.warnings:
            s += "; WARNING " + "; ".join(self.warnings)
        if self.failures:
            f = self.failures[0]
            s += f"; first failure {f.key} vector {f.i}: {f.why}"
        return s


class VectorRunner:
    def __init__(self, lib: L.Lib | None = None, map_exe: bool = True):
        self.lib = lib or L.load()
        self.gs = VL.GsMap()
        self.map_exe = map_exe
        self.cov = coverage_table()
        self.funcs = self.lib.functions()

    # ---------------------------------------------------------------- locations
    def resolve(self, st: L.State, bases: list[int], loc: dict) -> int | None:
        if "null" in loc:
            return 0
        if "sym" in loc:
            return st.data_base + self.gs.gs_off(loc["sym"], loc["off"])
        if "block" in loc:
            return bases[loc["block"]] + loc["off"]
        if "exe" in loc:
            sec = next((s for s in exe_sections() if s[0] == loc["exe"]), None)
            return None if sec is None else sec[1] + loc["off"]
        return None                               # raw: not representable here

    def loc_back(self, st: L.State, bases: list[int], sizes: list[int], va: int) -> dict:
        if va == 0:
            return {"null": True}
        off = va - st.data_base
        if 0 <= off < VL.DATA_SIZE:
            it = self.gs.at(off)
            return {"sym": it[2], "off": off - it[0]}
        for i, b in enumerate(bases):
            if b <= va < b + sizes[i]:
                return {"block": i, "off": va - b}
        for name, sva, data in exe_sections():
            if sva <= va < sva + len(data):
                return {"exe": name, "off": va - sva}
        return {"raw": va}

    # ---------------------------------------------------------------- one key
    def _setup(self, st: L.State, key: dict, v: dict, bases: list[int]):
        st.clear()
        st.unmap_all()
        if self.map_exe:
            for name, va, data in exe_sections():
                lo, hi = va, va + len(data)
                if lo < st.data_base + VL.DATA_SIZE and st.data_base < hi:
                    continue                          # .data is the GameState
                st.map(va, data, readonly=True)
        bufs = []
        for i, b in enumerate(key["blocks"]):
            raw = bytes.fromhex(b["hex"])
            buf = (C.c_uint8 * max(1, len(raw))).from_buffer_copy(raw.ljust(max(1, len(raw)), b"\0"))
            st.map(bases[i], buf)
            bufs.append(buf)
        for src in (key["seed"], v.get("in", [])):
            for r in src:
                if "sym" in r:
                    st.write(self.gs.gs_off(r["sym"], r["off"]), bytes.fromhex(r["hex"]))
                elif "block" in r:
                    h = bytes.fromhex(r["hex"])
                    C.memmove(C.addressof(bufs[r["block"]]) + r["off"], h, len(h))
        for r in key.get("relocs", []):
            nv = struct.pack("<I", self.resolve(st, bases, r["to"]))
            at = r["at"]
            if "block" in at:
                C.memmove(C.addressof(bufs[at["block"]]) + at["off"], nv, 4)
            else:
                st.write(self.gs.gs_off(at["sym"], at["off"]), nv)
        return bufs

    def _args(self, st, key, v, bases) -> list[int] | None:
        out = []
        for t, a in zip(key["args"], v["args"]):
            if "ptr" in a:
                va = self.resolve(st, bases, a["ptr"])
                if va is None:
                    return None
                out.append(va)
            elif "int64" in a:
                q = a["int64"] & 0xFFFFFFFFFFFFFFFF
                out += [q & 0xFFFFFFFF, q >> 32]
            elif "float" in a:
                if t == "double":
                    lo, hi = struct.unpack("<II", struct.pack("<d", a["float"]))
                    out += [lo, hi]
                else:
                    out.append(struct.unpack("<I", struct.pack("<f", a["float"]))[0])
            else:
                out.append(a["int"] & 0xFFFFFFFF)
        return out

    def _expected_out(self, st, v, bases) -> list[tuple[str, int, int, bytes]]:
        """[(space, index, offset, bytes)]: space 'gs' (GameState offset) or 'blk' (block index, offset)."""
        exp = []
        for r in v["out"]:
            b = bytearray.fromhex(r["hex"])
            where = "sym" if "sym" in r else "block"
            for op in v.get("out_ptrs", []):
                at = op["at"]
                if where not in at or at[where] != r[where]:
                    continue
                rel = at["off"] - r["off"]
                if 0 <= rel and rel + 4 <= len(b):
                    b[rel:rel + 4] = struct.pack("<I", self.resolve(st, bases, op["to"]))
            if where == "sym":
                exp.append(("gs", 0, self.gs.gs_off(r["sym"], r["off"]), bytes(b)))
            else:
                exp.append(("blk", r["block"], r["off"], bytes(b)))
        return exp

    def _ret_check(self, st, key, v, bases, sizes, r: L.SgRet) -> str | None:
        t = key["ret"]
        if t == "void":
            return None
        exp = v["ret"]
        if t in ("double", "float"):
            if not r.has_st0:
                return "no st(0) result"
            got = r.st0
            if isinstance(exp, dict) and "float_repr" in exp:
                exp = float(exp["float_repr"])
            return None if VL.float_eq(got, float(exp)) else f"ret {got!r} != {exp!r}"
        if t == "pointer":
            got = {"ptr": self.loc_back(st, bases, sizes, r.eax)}
            return None if got == exp else f"ret {got} != {exp}"
        if "ret_loc" in v:
            got = self.loc_back(st, bases, sizes, r.eax)
            return None if got == v["ret_loc"] else f"ret {got} != {v['ret_loc']}"
        got = VL.norm_ret(t, r.eax, r.edx)
        return None if got == exp else f"ret {got} != {exp}"

    def run_doc(self, doc: dict, name: str | None = None) -> FunctionResult:
        """Run every key of a .vec document (also an in-memory, e.g. corrupted, copy) against port function `name`."""
        fn = name or doc["function"]
        res = FunctionResult(fn, fn in self.funcs, vec_file=True, keys=len(doc["keys"]))
        cov = self.cov.get(doc["function"], {})
        res.coverage = cov.get("verdict", "-")
        if cov.get("distinct_results", "").isdigit():
            res.distinct_corpus = int(cov["distinct_results"])
        distinct = set()
        for key in doc["keys"]:
            for v in key["vectors"]:
                res.total += 1
                if v["status"] == "ok":
                    res.ok += 1
        if not res.implemented:
            return res
        info = self.funcs[fn]
        with self.lib.state() as st:
            st.set_data_base(VL.DATA_VA)
            if (ROOT / "original").is_dir():          # phase 3.5: functions that read the user's story files get them
                st.files_root(ROOT / "original")
            for key in doc["keys"]:
                sizes = [b["size"] for b in key["blocks"]]
                bases = block_bases(sizes)
                fpcw_ok = True
                if key.get("x87") or info["fpcw"]:
                    fpcw_ok = self.lib.set_fpcw(key["fpcw"]) == L.SG_OK
                if self.map_exe:
                    bad_const = self._const_mismatch(key)
                else:
                    bad_const = None
                for v in key["vectors"]:
                    if v["status"] != "ok":
                        res.skipped[f"status {v['status']}"] += 1
                        continue
                    if v.get("reloc_check") is False:
                        res.skipped["reloc_check false"] += 1
                        continue
                    if not fpcw_ok:
                        res.skipped[f"fpcw 0x{key['fpcw']:04x} unsupported"] += 1
                        continue
                    if bad_const:
                        res.skipped["exe constants differ"] += 1
                        continue
                    bufs = self._setup(st, key, v, bases)
                    args = self._args(st, key, v, bases)
                    if args is None:
                        res.skipped["raw location"] += 1
                        continue
                    if len(args) != info["arity"]:
                        res.run += 1
                        res.failed += 1
                        res.failures.append(VectorFailure(key["key"], v["i"],
                                                          f"arity {info['arity']} != corpus {len(args)} slots"))
                        continue
                    exp_out = self._expected_out(st, v, bases)
                    distinct.add(json.dumps([v["ret"], v.get("ret_loc"), [o[3].hex() for o in exp_out]]))
                    before_gs = np.frombuffer(st.image(), np.uint8)
                    before_blk = [bytes(b) for b in bufs]
                    res.run += 1
                    why = None
                    try:
                        r = st.call(fn, args)
                    except L.SgError as e:
                        why = str(e)
                    if why is None:
                        why = self._ret_check(st, key, v, bases, sizes, r)
                    if why is None:
                        why = self._out_check(st, bufs, exp_out, before_gs, before_blk)
                    if why:
                        res.failed += 1
                        res.failures.append(VectorFailure(key["key"], v["i"], why))
                    else:
                        res.passed += 1
            st.unmap_all()
        res.distinct = len(distinct)
        if res.run and res.run < MIN_VECTORS:
            res.warnings.append(f"only {res.run} vectors (< {MIN_VECTORS})")
        if res.run and res.distinct <= 1:
            res.warnings.append("zero spread: every vector gives the same result")
        if res.coverage not in ("full", "-"):
            res.warnings.append(f"branch coverage {res.coverage}: the segments must cover the rest")
        return res

    def _const_mismatch(self, key) -> bool:
        secs = {n: (va, d) for n, va, d in exe_sections()}
        for c in key.get("const", []):
            if c.get("exe") not in secs:
                continue
            d = secs[c["exe"]][1]
            h = bytes.fromhex(c["hex"])
            if bytes(d[c["off"]:c["off"] + len(h)]) != h:
                return True
        return False

    def _out_check(self, st, bufs, exp_out, before_gs, before_blk) -> str | None:
        after = np.frombuffer(st.image(), np.uint8)
        allowed_gs = np.zeros(len(after), bool)
        allowed_blk = [np.zeros(max(1, len(b)), bool) for b in before_blk]
        for space, idx, off, data in exp_out:
            if space == "gs":
                got = bytes(after[off:off + len(data)])
                allowed_gs[off:off + len(data)] = True
                if got != data:
                    k = next(i for i in range(len(data)) if got[i] != data[i])
                    sym, o = self.gs.at(off + k)[2], off + k - self.gs.at(off + k)[0]
                    return f"out {sym}+0x{o:x}: 0x{got[k]:02x} != 0x{data[k]:02x}"
            else:
                got = bytes(bufs[idx])[off:off + len(data)]
                allowed_blk[idx][off:off + len(data)] = True
                if got != data:
                    k = next(i for i in range(len(data)) if got[i] != data[i])
                    return f"out block {idx}+0x{off + k:x}: 0x{got[k]:02x} != 0x{data[k]:02x}"
        changed = np.nonzero((after != before_gs) & ~allowed_gs)[0]
        if len(changed):
            o = int(changed[0])
            it = self.gs.at(o)
            return f"extra write {it[2]}+0x{o - it[0]:x} ({len(changed)} bytes outside the out regions)"
        for i, b in enumerate(bufs):
            a = np.frombuffer(bytes(b), np.uint8)
            c = np.nonzero((a != np.frombuffer(before_blk[i], np.uint8)) & ~allowed_blk[i][:len(a)])[0]
            if len(c):
                return f"extra write block {i}+0x{int(c[0]):x} ({len(c)} bytes outside the out regions)"
        return None

    def run_function(self, function: str) -> FunctionResult:
        p = vec_path(function)
        if not p.exists():
            r = FunctionResult(function, function in self.funcs, vec_file=False)
            r.coverage = self.cov.get(function, {}).get("verdict", "-")
            return r
        return self.run_doc(VL.read_vec(p), function)


def manifest_functions() -> list[str]:
    with MANIFEST_TSV.open(newline="") as f:
        seen = []
        for r in csv.DictReader(f, delimiter="\t"):
            if r["function"] not in seen:
                seen.append(r["function"])
    return seen
