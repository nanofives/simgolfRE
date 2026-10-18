r"""GameState byte images of the original's state, per-object hashes and diffs (phase 2.2, 2026-10-18).

A STATE IMAGE is the GameState of port/include/gamestate.h as bytes (golf_clean.exe's .data section, 0x381174 bytes) plus
a KNOWN mask (which bytes the source holds). Sources:
  full     a whole .data dump: scenario_ab.py `<name>.full.bin` (C3FT_FULLDUMP=1 / --obj-image), restart_ab.py
           `<name>.full.bin` / `<k>.pre.bin`, or a frame-head capture `<k>.fh.bin` (.data then the stack: the first
           0x381174 bytes are used);
  dump     a scenario_ab.py checkpoint dump `<name>.bin`: the .sve globals (re/tools/sve.py FIELDS), both tail blocks
           and the two extra words (scenario_ab.py regions(), in that order);
  sve      a save file, split through re/model/model_ctypes.py with the field table re/model/sve_fields.tsv (body fields
           present per the flag word; the 100-byte header is the first 100 bytes of g_textBuffer);
  state    this tool's own `<x>.state.bin` + `<x>.state.json` (convert).
The object table is read from the generated port/include/gamestate_meta.inc and gamestate_addresses.inc (the same tables
the port compiles), so every tool agrees on symbol, offset, size and class.

OBJECT HASH (sgh64): the object's bytes as little-endian dwords (the last one zero-padded), h = sum over i of
(w_i ^ 0x5bd1e995) * ((2i + 1) * 0x9e3779b97f4a7c15) mod 2^64, then h ^= byte length and the splitmix64 finaliser. One
changed dword always changes the sum (odd multiplier). SG_POINTER_WORD dwords are hashed as 0. scenario_det.js's
frame-head tracer computes the same function natively (CModule), so its hashes compare with these.

    ~/bin/py -3.12 re/tools/state_image.py hash <file> [--kind full|dump|sve|state] [--classes sim] [--json out.json]
    ~/bin/py -3.12 re/tools/state_image.py diff <a> <b> [--classes sim,hidden] [--all] [--json out.json]
    ~/bin/py -3.12 re/tools/state_image.py convert <file> --out <x>       # writes <x>.state.bin / <x>.state.json
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "re" / "model"))

META = ROOT / "port" / "include" / "gamestate_meta.inc"
ADDRS = ROOT / "port" / "include" / "gamestate_addresses.inc"
SVE_FIELDS = ROOT / "re" / "model" / "sve_fields.tsv"
EXTRA_REGIONS = [(0x00822D9C, 4), (0x0051B380, 4)]      # scenario_ab.py EXTRA_REGIONS (rng_seed, seed_time)
MASK64 = (1 << 64) - 1
K_MUL = 0x9E3779B97F4A7C15
K_XOR = 0x5BD1E995


# ---------------------------------------------------------------- layout (from the generated tables)
class Layout:
    def __init__(self, meta: pathlib.Path = META, addrs: pathlib.Path = ADDRS):
        a = addrs.read_text(encoding="utf-8")
        m = re.search(r"^SG_DATA_SECTION\((0x[0-9a-f]+), (0x[0-9a-f]+)\)", a, re.M)
        self.base, self.n = int(m.group(1), 16), int(m.group(2), 16)
        self.objs = []
        self.words: dict[str, list[int]] = {}
        self.gaps = []
        for line in meta.read_text(encoding="utf-8").splitlines():
            mo = re.match(r"SG_OBJECT\((\w+), (0x[0-9a-f]+), (0x[0-9a-f]+), (\w+), (\w+), (\w+), (\d)\)$", line)
            if mo:
                self.objs.append({"symbol": mo.group(1), "offset": int(mo.group(2), 16), "size": int(mo.group(3), 16),
                                  "cls": mo.group(4), "pointer": mo.group(5), "hidden": mo.group(6),
                                  "saved": int(mo.group(7))})
                continue
            mw = re.match(r"SG_POINTER_WORD\((\w+), (0x[0-9a-f]+)\)$", line)
            if mw:
                self.words.setdefault(mw.group(1), []).append(int(mw.group(2), 16))
                continue
            mg = re.match(r"SG_GAP\((0x[0-9a-f]+), (0x[0-9a-f]+)\)$", line)
            if mg:
                self.gaps.append((int(mg.group(1), 16), int(mg.group(2), 16)))
        self.by = {o["symbol"]: o for o in self.objs}
        self.idx = np.full(self.n, -1, np.int32)
        for i, o in enumerate(self.objs):
            self.idx[o["offset"]:o["offset"] + o["size"]] = i
        self.word_mask = np.zeros(self.n, bool)          # bytes of SG_POINTER_WORD dwords
        for s, offs in self.words.items():
            for w in offs:
                lo = self.by[s]["offset"] + w
                self.word_mask[lo:lo + 4] = True

    def offset(self, va: int) -> int:
        if not self.base <= va < self.base + self.n:
            raise ValueError(f"0x{va:08x} is outside the data section")
        return va - self.base

    def symbol_at(self, off: int) -> str:
        i = int(self.idx[off])
        return self.objs[i]["symbol"] if i >= 0 else f"gap_{off:06x}"


_LAYOUT = None


def layout() -> Layout:
    global _LAYOUT
    if _LAYOUT is None:
        _LAYOUT = Layout()
    return _LAYOUT


# ---------------------------------------------------------------- hash
def _mix(h: int) -> int:
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9 & MASK64
    h = (h ^ (h >> 27)) * 0x94D049BB133111EB & MASK64
    return h ^ (h >> 31)


def sgh64(data: bytes) -> int:
    n = len(data)
    pad = (-n) % 4
    w = np.frombuffer(bytes(data) + b"\0" * pad, "<u4").astype(np.uint64)
    i = np.arange(len(w), dtype=np.uint64)
    with np.errstate(over="ignore"):
        mul = (i * np.uint64(2) + np.uint64(1)) * np.uint64(K_MUL)
        s = int(((w ^ np.uint64(K_XOR)) * mul).sum(dtype=np.uint64))
    return _mix((s ^ n) & MASK64)


# ---------------------------------------------------------------- images
class StateImage:
    def __init__(self, data=None, known=None, source=""):
        L = layout()
        self.data = bytearray(L.n) if data is None else bytearray(data)
        self.known = np.zeros(L.n, bool) if known is None else np.asarray(known, bool).copy()
        self.source = source

    def put(self, va: int, blob: bytes):
        off = layout().offset(va)
        if off + len(blob) > layout().n:
            raise ValueError(f"0x{va:08x}+0x{len(blob):x} runs past the data section")
        self.data[off:off + len(blob)] = blob
        self.known[off:off + len(blob)] = True

    def object_bytes(self, o: dict, masked=True) -> bytes:
        b = bytearray(self.data[o["offset"]:o["offset"] + o["size"]])
        if masked:
            for w in layout().words.get(o["symbol"], ()):
                b[w:w + 4] = b"\0\0\0\0"
        return bytes(b)

    def coverage(self, o: dict) -> str:
        k = self.known[o["offset"]:o["offset"] + o["size"]]
        return "full" if k.all() else ("none" if not k.any() else "partial")

    def hashes(self, classes=None) -> dict:
        out = {}
        for o in layout().objs:
            if classes and o["cls"] not in classes:
                continue
            cov = self.coverage(o)
            out[o["symbol"]] = {"cls": o["cls"], "coverage": cov,
                                "hash": f"{sgh64(self.object_bytes(o)):016x}" if cov != "none" else None}
        return out

    def save(self, stem: pathlib.Path):
        L = layout()
        pathlib.Path(str(stem) + ".state.bin").write_bytes(bytes(self.data))
        runs = _runs(self.known)
        meta = {"source": self.source, "base": f"0x{L.base:08x}", "size": f"0x{L.n:x}",
                "known": [[f"0x{a:x}", f"0x{b - a:x}"] for a, b in runs], "hashes": self.hashes()}
        pathlib.Path(str(stem) + ".state.json").write_text(json.dumps(meta, indent=1))


def _runs(mask) -> list[tuple[int, int]]:
    m = np.concatenate([[False], np.asarray(mask, bool), [False]]).astype(np.int8)
    d = np.diff(m)
    return list(zip(np.nonzero(d == 1)[0].tolist(), np.nonzero(d == -1)[0].tolist()))


def from_full(path: pathlib.Path) -> StateImage:
    L = layout()
    blob = pathlib.Path(path).read_bytes()
    if len(blob) < L.n:
        raise ValueError(f"{path}: {len(blob)} bytes, a .data image is 0x{L.n:x}")
    return StateImage(blob[:L.n], np.ones(L.n, bool), f"full:{path}")


def dump_regions() -> list[tuple[int, int]]:
    import sve
    regs = list(sve.FIELDS)
    for _bit, extra in (sve.TAIL_A, sve.TAIL_B):
        regs += list(extra)
    return regs + EXTRA_REGIONS


def from_dump(path: pathlib.Path) -> StateImage:
    blob = pathlib.Path(path).read_bytes()
    regs = dump_regions()
    total = sum(n for _va, n in regs)
    if len(blob) != total:
        raise ValueError(f"{path}: {len(blob)} bytes, a checkpoint dump is {total}")
    img = StateImage(source=f"dump:{path}")
    pos = 0
    for va, n in regs:
        img.put(va, blob[pos:pos + n])
        pos += n
    return img


def _ctypes_mirror():
    import model_ctypes as M
    return M


def from_sve(path: pathlib.Path) -> StateImage:
    """A .sve through the model mirror (model_ctypes.split_sve / join_sve over sve_fields.tsv); every present body field
    is placed at its table va, the last 100-byte header at g_textBuffer."""
    import csv
    M = _ctypes_mirror()
    blob = pathlib.Path(path).read_bytes()
    table = M.load_field_table(str(SVE_FIELDS))
    headers, parts = M.split_sve(blob, table)
    if M.join_sve(headers, parts, table) != blob:
        raise ValueError(f"{path}: the field table does not round-trip this file")
    with SVE_FIELDS.open(newline="", encoding="utf-8") as f:
        va_of = {r["index"]: int(r["va"], 16) for r in csv.DictReader(f, delimiter="\t")}
    flags_idx = next(r["flag_field"] for r in table if r["flag_field"])
    flags_row = next(r for r in table if r["index"] == flags_idx)
    fkey = M.field_parts(flags_row, b"\0" * flags_row["size"])[0][0]
    flags = int.from_bytes(bytes(parts[fkey])[:4], "little")
    img = StateImage(source=f"sve:{path}")
    hdr = next(r for r in table if r["part"] == "header")
    img.put(va_of[hdr["index"]], headers[-1])
    pos = len(headers) * hdr["size"]
    for r in M.body_layout(table, flags):
        img.put(va_of[r["index"]], blob[pos:pos + r["size"]])
        pos += r["size"]
    assert pos == len(blob)
    return img


def from_state(path: pathlib.Path) -> StateImage:
    stem = str(path)[:-len(".state.bin")] if str(path).endswith(".state.bin") else str(path)
    data = pathlib.Path(stem + ".state.bin").read_bytes()
    meta = json.loads(pathlib.Path(stem + ".state.json").read_text())
    known = np.zeros(layout().n, bool)
    for a, n in meta["known"]:
        known[int(a, 16):int(a, 16) + int(n, 16)] = True
    return StateImage(data, known, meta.get("source", f"state:{path}"))


def guess_kind(path: pathlib.Path) -> str:
    s = str(path).lower()
    if s.endswith(".sve"):
        return "sve"
    if s.endswith(".state.bin") or s.endswith(".state.json"):
        return "state"
    if s.endswith((".full.bin", ".fh.bin", ".pre.bin")):
        return "full"
    if s.endswith(".bin"):
        return "dump"
    raise ValueError(f"{path}: give --kind")


def load(path, kind=None) -> StateImage:
    path = pathlib.Path(path)
    kind = kind or guess_kind(path)
    return {"full": from_full, "dump": from_dump, "sve": from_sve, "state": from_state}[kind](path)


# ---------------------------------------------------------------- diff
def diff(a: StateImage, b: StateImage, classes=None, include_gaps=False) -> list[dict]:
    """Objects whose bytes differ where both images know them (pointer words masked). One row per object: symbol, class,
    first differing byte (offset in the object), differing byte count, coverage. Gaps (unmodeled bytes) are reported per
    gap when include_gaps."""
    L = layout()
    da = np.frombuffer(bytes(a.data), np.uint8)
    db = np.frombuffer(bytes(b.data), np.uint8)
    neq = (da != db) & a.known & b.known & ~L.word_mask
    out = []
    for off in np.nonzero(neq)[0]:
        off = int(off)
        i = int(L.idx[off])
        if i < 0:
            if not include_gaps:
                continue
            key, cls, lo = L.symbol_at(off), "unmodeled", None
        else:
            o = L.objs[i]
            if classes and o["cls"] not in classes:
                continue
            key, cls, lo = o["symbol"], o["cls"], o["offset"]
        if out and out[-1]["symbol"] == key:
            out[-1]["bytes"] += 1
            continue
        out.append({"symbol": key, "cls": cls, "first": off - lo if lo is not None else 0, "bytes": 1,
                    "offset": f"0x{off:06x}"})
    for r in out:
        if r["cls"] != "unmodeled":
            o = L.by[r["symbol"]]
            r["coverage"] = "both full" if a.coverage(o) == b.coverage(o) == "full" else \
                f"{a.coverage(o)}/{b.coverage(o)}"
    return out


# ---------------------------------------------------------------- CLI
def _classes(s):
    return set(s.split(",")) if s else None


def cmd_hash(a):
    img = load(a.file, a.kind)
    h = img.hashes(_classes(a.classes))
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps({"source": img.source, "hashes": h}, indent=1))
    full = sum(1 for v in h.values() if v["coverage"] == "full")
    print(f"{img.source}: {len(h)} objects, {full} fully known")
    if not a.json:
        for k, v in h.items():
            if v["hash"]:
                print(f"  {k:40s} {v['cls']:8s} {v['coverage']:8s} {v['hash']}")
    return 0


def cmd_diff(a):
    ia, ib = load(a.a, a.kind_a), load(a.b, a.kind_b)
    rows = diff(ia, ib, _classes(a.classes), include_gaps=a.all)
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps({"a": ia.source, "b": ib.source, "rows": rows}, indent=1))
    print(f"{len(rows)} objects differ ({ia.source} vs {ib.source})")
    for r in rows:
        print(f"  {r['symbol']:40s} {r['cls']:9s} first +0x{r['first']:x} ({r['offset']}) bytes {r['bytes']} "
              f"{r.get('coverage', '')}")
    return 1 if rows else 0


def cmd_convert(a):
    img = load(a.file, a.kind)
    img.save(pathlib.Path(a.out))
    print(f"wrote {a.out}.state.bin / .state.json ({int(img.known.sum())} bytes known)")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("hash"); p.add_argument("file"); p.add_argument("--kind"); p.add_argument("--classes")
    p.add_argument("--json")
    p = sp.add_parser("diff"); p.add_argument("a"); p.add_argument("b"); p.add_argument("--kind-a")
    p.add_argument("--kind-b"); p.add_argument("--classes"); p.add_argument("--all", action="store_true")
    p.add_argument("--json")
    p = sp.add_parser("convert"); p.add_argument("file"); p.add_argument("--kind"); p.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    return {"hash": cmd_hash, "diff": cmd_diff, "convert": cmd_convert}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
