"""Function-vector corpus (phase 2.5): schema, reader / writer, the GameState address map and the Unicorn engine.

A `.vec` file (port/corpus/vectors/<function>.vec, gitignored) is one gzip-compressed JSON document holding every
captured vector of one function of golf_clean.exe, grouped by registry key. Schema: re/model/vector_schema.md.
Memory is addressed by GameState symbol + offset (port/include/gamestate_meta.inc), by fixture block id + offset, or
by exe section + offset (constants read from the user's own exe); a raw address appears only in the `tooling`
members, which the port harness never reads.

Used by re/tools/vector_capture.py (capture + build), re/tools/vector_validate.py (replay checks) and the tests.
"""
from __future__ import annotations

import bisect
import gzip
import hashlib
import json
import os
import pathlib
import re
import struct

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCHEMA = "simgolf-vec/1"
DATA_VA, DATA_SIZE = 0x004C1000, 0x381174
IMAGE_BASE = 0x00400000
META_INC = ROOT / "port" / "include" / "gamestate_meta.inc"
EXE = ROOT / "original" / "golf_clean.exe"
VEC_DIR = pathlib.Path(os.environ.get("SIMGOLF_VEC_DIR") or ROOT / "port" / "corpus" / "vectors")   # env: tests

# Frida NativeFunction type -> (size on the stack, struct code); 'void' only as a return type
ARG_TYPES = {"int": 4, "uint": 4, "uint32": 4, "int32": 4, "pointer": 4, "uint8": 4, "int8": 4, "uint16": 4, "int16": 4,
             "bool": 4, "char": 4, "uchar": 4, "short": 4, "ushort": 4, "long": 4, "ulong": 4,
             "float": 4, "double": 8, "int64": 8, "uint64": 8}
RET_TYPES = set(ARG_TYPES) | {"void"}


# --------------------------------------------------------------------------------------------- the address map
_META_RE = re.compile(r"^SG_OBJECT\((\w+), 0x([0-9a-f]+), 0x([0-9a-f]+), (\w+), ([\w:+,]+|none|type), (\w+), (\d)\)")
_GAP_RE = re.compile(r"^SG_GAP\(0x([0-9a-f]+), 0x([0-9a-f]+)\)")


class GsMap:
    """GameState objects and gaps by offset. A gap is addressed as symbol `gap_<offset6>` (the generated member name)."""

    def __init__(self, path: pathlib.Path = META_INC):
        items = []
        for line in path.read_text().splitlines():
            m = _META_RE.match(line)
            if m:
                items.append((int(m[2], 16), int(m[3], 16), m[1], m[4], m[5]))
                continue
            g = _GAP_RE.match(line)
            if g:
                off = int(g[1], 16)
                items.append((off, int(g[2], 16), f"gap_{off:06x}", "gap", "none"))
        items.sort()
        self.items = items
        self.starts = [i[0] for i in items]
        self.by_sym = {i[2]: i for i in items}
        end = 0
        for off, size, *_ in items:
            if off != end:
                raise ValueError(f"gamestate_meta.inc not contiguous at 0x{off:x} (expected 0x{end:x})")
            end = off + size
        if end != DATA_SIZE:
            raise ValueError(f"gamestate_meta.inc covers 0x{end:x}, not 0x{DATA_SIZE:x}")

    def at(self, off: int):
        i = bisect.bisect_right(self.starts, off) - 1
        return self.items[i]

    def ref(self, va: int) -> tuple[str, int]:
        off = va - DATA_VA
        if not 0 <= off < DATA_SIZE:
            raise ValueError(f"0x{va:08x} is not in the data section")
        it = self.at(off)
        return it[2], off - it[0]

    def va(self, sym: str, off: int) -> int:
        return DATA_VA + self.by_sym[sym][0] + off

    def gs_off(self, sym: str, off: int) -> int:
        return self.by_sym[sym][0] + off

    def split(self, va: int, data: bytes) -> list[dict]:
        """A data-section byte run as regions that each lie inside one object or gap."""
        out, pos = [], 0
        while pos < len(data):
            off = va - DATA_VA + pos
            it = self.at(off)
            n = min(len(data) - pos, it[0] + it[1] - off)
            out.append({"sym": it[2], "off": off - it[0], "hex": data[pos:pos + n].hex()})
            pos += n
        return out

    def classes(self) -> dict[str, str]:
        return {i[2]: i[3] for i in self.items}


# --------------------------------------------------------------------------------------------- byte-run helpers
def runs(addrs) -> list[tuple[int, int]]:
    """Sorted unique byte addresses -> [(start, length)]."""
    out = []
    for a in sorted(set(addrs)):
        if out and out[-1][0] + out[-1][1] == a:
            out[-1] = (out[-1][0], out[-1][1] + 1)
        else:
            out.append((a, 1))
    return out


def sha256_file(p: pathlib.Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


# --------------------------------------------------------------------------------------------- .vec I/O
def write_vec(path: pathlib.Path, doc: dict) -> None:
    validate_doc(doc)
    path.parent.mkdir(parents=True, exist_ok=True)
    raw = json.dumps(doc, separators=(",", ":"), sort_keys=False).encode()
    tmp = path.with_suffix(".vec.tmp")
    # no file name and mtime 0 in the gzip header: identical documents give identical files
    with open(tmp, "wb") as fh, gzip.GzipFile(filename="", mode="wb", fileobj=fh, compresslevel=6, mtime=0) as f:
        f.write(raw)
    tmp.replace(path)


def read_vec(path: pathlib.Path) -> dict:
    with gzip.open(path, "rb") as f:
        doc = json.loads(f.read())
    validate_doc(doc)
    return doc


class SchemaError(ValueError):
    pass


def _need(cond, msg):
    if not cond:
        raise SchemaError(msg)


def _check_loc(loc: dict, nblocks: int, where: str):
    kinds = [k for k in ("sym", "block", "exe", "null", "raw") if k in loc]
    _need(len(kinds) == 1, f"{where}: a location names exactly one of sym/block/exe/null/raw: {loc}")
    k = kinds[0]
    if k == "block":
        _need(0 <= loc["block"] < nblocks, f"{where}: block {loc['block']} out of range")
    if k in ("sym", "block", "exe"):
        _need(isinstance(loc.get("off"), int), f"{where}: location without an integer `off`")


def validate_doc(doc: dict) -> None:
    """Structural check of a .vec document (schema re/model/vector_schema.md)."""
    _need(doc.get("schema") == SCHEMA, f"schema {doc.get('schema')!r} != {SCHEMA}")
    for k in ("function", "module", "keys"):
        _need(k in doc, f"missing {k}")
    for key in doc["keys"]:
        for k in ("key", "abi", "ret", "args", "fpcw", "blocks", "seed", "vectors"):
            _need(k in key, f"key {key.get('key')}: missing {k}")
        _need(key["ret"] in RET_TYPES, f"key {key['key']}: ret type {key['ret']}")
        for t in key["args"]:
            _need(t in ARG_TYPES, f"key {key['key']}: arg type {t}")
        nb = len(key["blocks"])
        for i, b in enumerate(key["blocks"]):
            _need(b.get("id") == i and len(bytes.fromhex(b["hex"])) == b["size"], f"key {key['key']}: block {i} malformed")
        for r in key.get("relocs", []):
            _check_loc(r["at"], nb, f"key {key['key']} reloc")
            _check_loc(r["to"], nb, f"key {key['key']} reloc")
        for reg in key["seed"] + key.get("const", []):
            _check_loc(reg, nb, f"key {key['key']} seed")
            _need("hex" in reg, f"key {key['key']}: seed region without hex")
        for v in key["vectors"]:
            _need(len(v["args"]) == len(key["args"]), f"key {key['key']} vector {v.get('i')}: {len(v['args'])} args")
            for a in v["args"]:
                _need(len(a) == 1 and next(iter(a)) in ("int", "ptr", "float", "int64"), f"arg {a}")
                if "ptr" in a:
                    _check_loc(a["ptr"], nb, f"key {key['key']} vector {v['i']} arg")
            _need("ret" in v and "out" in v, f"key {key['key']} vector {v.get('i')}: ret/out missing")
            for reg in v["out"] + v.get("in", []):
                _check_loc(reg, nb, f"key {key['key']} vector {v['i']} region")


# --------------------------------------------------------------------------------------------- values
def norm_ret(t: str, eax: int, edx: int = 0, st0: float | None = None):
    """The return value of one call as the corpus stores it (what Frida's NativeFunction reports for that type)."""
    eax &= 0xFFFFFFFF
    if t == "void":
        return None
    if t in ("double", "float"):
        return st0
    if t in ("int64", "uint64"):
        v = (edx & 0xFFFFFFFF) << 32 | eax
        return v - (1 << 64) if t == "int64" and v >> 63 else v
    if t in ("uint8", "uchar", "bool"):
        return eax & 0xFF
    if t in ("int8", "char"):
        v = eax & 0xFF
        return v - 0x100 if v & 0x80 else v
    if t in ("uint16", "ushort"):
        return eax & 0xFFFF
    if t in ("int16", "short"):
        v = eax & 0xFFFF
        return v - 0x10000 if v & 0x8000 else v
    if t in ("int", "int32", "long"):
        return eax - (1 << 32) if eax >> 31 else eax
    return eax  # uint, uint32, ulong, pointer (pointer returns are turned into locations by the builder)


def f80_to_float(mant: int, exp: int) -> float:
    """An x87 80-bit value (Unicorn reports FPn as (mantissa, sign|exponent)) rounded to a Python float (double),
    as `fstp qword` does with round-to-nearest."""
    sign = -1.0 if exp & 0x8000 else 1.0
    e = exp & 0x7FFF
    if e == 0 and mant == 0:
        return 0.0 * sign
    if e == 0x7FFF:
        return sign * float("inf") if (mant & ((1 << 63) - 1)) == 0 else float("nan")
    import fractions
    v = fractions.Fraction(mant) * (fractions.Fraction(2) ** (e - 16383 - 63))
    return sign * float(v)   # Fraction -> float rounds to nearest, ties to even


def float_eq(a, b) -> bool:
    if a is None or b is None:
        return a is b
    if isinstance(a, float) and isinstance(b, float):
        if a != a and b != b:
            return True
        return struct.pack("<d", a) == struct.pack("<d", b)
    return a == b
