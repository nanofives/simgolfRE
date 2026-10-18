r"""Generate re/model/model_ctypes.py: a Python ctypes mirror of the phase-1 data model (stream G2, 2026-10-18).

Inputs
  re/model/include/sections/*.h    the six stream headers (clean C99, fixed-width types, no addresses)
  re/model/{b,c,d,e1,e2,f}_addresses.tsv   symbol -> element type and size (the va column is NOT copied)
  re/model/sve_fields.tsv          which symbols the .sve fields map onto (only to report coverage here)

The headers are parsed by a small C reader that covers what they contain (checked: no unions, bit-fields or pointer
members): `#define NAME <int expr>`, named and anonymous enums, `typedef struct N { T m[dims]; ... } N;`,
`typedef T N[dims];`, and the `*_STATIC_ASSERT(sizeof(T) == N | offsetof(T, m) == N, tag)` lines. Layout follows MSVC's
default packing for these types (every member at a multiple of its own alignment, alignment = scalar size, struct
alignment = largest member's), which is also what ctypes does with no _pack_. The generator fails when:
  - a static assert of a header does not hold for the parsed layout,
  - a member's `+0xNN` offset comment differs from its computed offset,
  - ctypes' sizeof / offset of a generated class differs from the computed one.

The generated module embeds no original address: it maps model SYMBOL names to element types and shapes, and works on a
dict symbol -> bytes. Its .sve helpers read re/model/sve_fields.tsv for the field order and sizes only.

    ~/bin/py -3.12 re/tools/model_ctypes_gen.py            # writes re/model/model_ctypes.py, prints the checks
    ~/bin/py -3.12 re/tools/model_ctypes_gen.py --check    # exit 1 if model_ctypes.py is stale
"""
from __future__ import annotations

import argparse
import csv
import ctypes
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SECTIONS = ROOT / "re" / "model" / "include" / "sections"
STREAMS = ["b", "c", "d", "e1", "e2", "f"]
OUT = ROOT / "re" / "model" / "model_ctypes.py"

SCALARS = {  # C type -> (ctypes name, size)
    "int8_t": ("c_int8", 1), "uint8_t": ("c_uint8", 1), "char": ("c_char", 1), "int16_t": ("c_int16", 2),
    "uint16_t": ("c_uint16", 2), "int32_t": ("c_int32", 4), "uint32_t": ("c_uint32", 4), "float": ("c_float", 4),
    "double": ("c_double", 8), "int64_t": ("c_int64", 8), "uint64_t": ("c_uint64", 8),
}
# spellings the address maps use for element types (32-bit pointers, vtable words and flag words are uint32 here)
ALIASES = {
    "int8": "int8_t", "uint8": "uint8_t", "int16": "int16_t", "uint16": "uint16_t", "int32": "int32_t",
    "uint32": "uint32_t", "flags32": "uint32_t", "flags8": "uint8_t", "ptr": "uint32_t", "pointer": "uint32_t",
    "vtable": "uint32_t", "vbtable": "uint32_t",
}


# ------------------------------------------------------------------------------------------------ C reader
def strip_comments_keep_offsets(text: str) -> str:
    """Comments removed, but a member's `/* +0xNN` offset comment is kept as a marker `@OFF(0xNN)` after its `;`."""
    text = re.sub(r";[ \t]*/\*\s*\+(0x[0-9a-fA-F]+)", r"; @OFF(\1) /*", text)
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


class Model:
    def __init__(self):
        self.consts: dict[str, int] = {}
        self.structs: dict[str, dict] = {}      # name -> {fields: [(name, base, dims, off_comment)], file}
        self.arrays: dict[str, tuple] = {}      # typedef name -> (base, dims)
        self.asserts: list[tuple] = []          # (file, kind, type, member, value, text)
        self.layout: dict[str, dict] = {}       # struct -> {size, align, offsets{member: off}}
        self.order: list[str] = []

    # ---- expressions
    def eval(self, expr: str) -> int:
        e = expr.strip()
        e = re.sub(r"\b(0x[0-9a-fA-F]+|\d+)[uUlL]+\b", r"\1", e)

        def name(m):
            n = m.group(0)
            if n in self.consts:
                return str(self.consts[n])
            raise KeyError(n)
        e = re.sub(r"\b[A-Za-z_]\w*\b", name, e)
        if not re.fullmatch(r"[0-9a-fA-Fx\s()+\-*/|&<>~^]+", e):
            raise ValueError(expr)
        return int(eval(e.replace("/", "//"), {"__builtins__": {}}))  # noqa: S307 (digits and operators only)

    def parse(self, path: pathlib.Path):
        raw = path.read_text(encoding="utf-8")
        text = strip_comments_keep_offsets(raw)
        for m in re.finditer(r"^[ \t]*#define[ \t]+(\w+)[ \t]+([^\n]+)$", text, re.M):
            try:
                self.consts[m.group(1)] = self.eval(m.group(2))
            except (KeyError, ValueError, SyntaxError):
                pass
        for m in re.finditer(r"\benum\b\s*(\w*)\s*\{(.*?)\}\s*;", text, re.S):
            cur = -1
            for item in m.group(2).split(","):
                item = item.strip()
                if not item:
                    continue
                if "=" in item:
                    n, v = item.split("=", 1)
                    cur = self.eval(v)
                else:
                    n, cur = item, cur + 1
                self.consts[n.strip()] = cur
        body = re.sub(r"^[ \t]*#[^\n]*$", " ", text, flags=re.M)
        for m in re.finditer(r"typedef\s+struct\s+(\w*)\s*\{(.*?)\}\s*(\w+)\s*;", body, re.S):
            name = m.group(3)
            fields = []
            for part in re.split(r";", m.group(2)):
                off = re.match(r"\s*@OFF\((0x[0-9a-fA-F]+)\)", part)
                if off and fields:
                    f = fields[-1]
                    fields[-1] = (f[0], f[1], f[2], int(off.group(1), 16))
                    part = part[off.end():]
                decl = " ".join(part.split())
                if not decl:
                    continue
                dm = re.fullmatch(r"(?:struct\s+|enum\s+)?(\w+)\s+(\w+)((?:\s*\[[^\]]+\])*)", decl)
                if not dm:
                    raise SystemExit(f"{path.name}: {name}: cannot read member `{decl}`")
                dims = [self.eval(d) for d in re.findall(r"\[([^\]]+)\]", dm.group(3))]
                fields.append((dm.group(2), dm.group(1), dims, None))
            if name in self.structs:
                raise SystemExit(f"{path.name}: struct {name} also defined in {self.structs[name]['file']}")
            self.structs[name] = {"fields": fields, "file": path.name}
            self.order.append(name)
        for m in re.finditer(r"typedef\s+(?!struct\b)(\w+)\s+(\w+)((?:\s*\[[^\]]+\])+)\s*;", body):
            if "static_assert" in m.group(2):
                continue
            self.arrays[m.group(2)] = (m.group(1), [self.eval(d) for d in re.findall(r"\[([^\]]+)\]", m.group(3))])
        for m in re.finditer(r"\b\w*STATIC_ASSERT\s*\(\s*(sizeof|offsetof)\s*\(([^)]*)\)\s*==\s*([^,]+),", body):
            args = [a.strip() for a in m.group(2).split(",")]
            if args[0] in ("cond",):
                continue
            self.asserts.append((path.name, m.group(1), args[0], args[1] if len(args) > 1 else None,
                                 self.eval(m.group(3)), m.group(0)))

    # ---- layout
    def size_align(self, base: str) -> tuple[int, int]:
        base = ALIASES.get(base, base)
        if base in SCALARS:
            s = SCALARS[base][1]
            return s, s
        if base in self.arrays:
            b, dims = self.arrays[base]
            s, a = self.size_align(b)
            n = 1
            for d in dims:
                n *= d
            return s * n, a
        if base in self.structs:
            lay = self.compute(base)
            return lay["size"], lay["align"]
        raise KeyError(base)

    def compute(self, name: str, stack=()) -> dict:
        if name in self.layout:
            return self.layout[name]
        if name in stack:
            raise SystemExit(f"recursive struct {name}")
        off, align, offs = 0, 1, {}
        for fname, base, dims, _c in self.structs[name]["fields"]:
            if ALIASES.get(base, base) not in SCALARS and base not in self.arrays:
                self.compute(base, stack + (name,))
            s, a = self.size_align(base)
            n = 1
            for d in dims:
                n *= d
            off = (off + a - 1) // a * a
            offs[fname] = off
            off += s * n
            align = max(align, a)
        size = (off + align - 1) // align * align
        self.layout[name] = {"size": size, "align": align, "offsets": offs}
        return self.layout[name]


def check_model(m: Model) -> list[str]:
    errs = []
    for s in m.order:
        lay = m.compute(s)
        for fname, _b, _d, oc in m.structs[s]["fields"]:
            if oc is not None and oc != lay["offsets"][fname]:
                errs.append(f"{m.structs[s]['file']}: {s}.{fname}: comment +0x{oc:x}, computed +0x{lay['offsets'][fname]:x}")
    for f, kind, t, mem, val, text in m.asserts:
        try:
            if kind == "sizeof":
                got = m.size_align(t)[0]
            else:
                got = m.compute(t)["offsets"][mem]
        except KeyError as e:
            errs.append(f"{f}: {text}: unknown {e}")
            continue
        if got != val:
            errs.append(f"{f}: {text}: computed 0x{got:x}")
    return errs


# ------------------------------------------------------------------------------------------------ address maps
def norm_type(t: str) -> tuple[str, list[int]] | None:
    """Element type text of an address map -> (base, dims) or None."""
    t = re.sub(r"\(.*?\)", "", t or "").strip()
    t = re.sub(r"^struct\s+", "", t)
    if not t or t.startswith("{"):
        return None
    if "*" in t:                                          # `char *[20]`, `uint16_t*`, `TerrainObject *`
        dims = [int(d, 0) for d in re.findall(r"\[(\w+)\]", t)]
        return "uint32_t", dims
    mm = re.fullmatch(r"(\w+(?: object)?)\s*((?:\[\s*\w+\s*\])*)", t)
    if not mm:
        return None
    dims = [int(d, 0) for d in re.findall(r"\[\s*(\w+)\s*\]", mm.group(2))]
    return ALIASES.get(mm.group(1), mm.group(1)), dims


def _map_rows():
    """(stream, row) of the MERGED address map re/model/addresses.tsv (the integrated truth: sizes and counts G1 corrected,
    rows it added). The six stream maps are inputs of the merge, not of the mirror."""
    with (ROOT / "re" / "model" / "addresses.tsv").open(newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="	"):
            yield (r.get("stream") or "g1"), r


def symbols(m: Model) -> tuple[dict, list[str]]:
    out, notes = {}, []
    if True:
        if True:
            for s, r in _map_rows():
                if (r.get("module") or "golf_clean.exe") != "golf_clean.exe":
                    continue
                try:
                    size = int(r["size"], 0)
                except (ValueError, TypeError):
                    continue
                nt = norm_type(r.get("element_type", ""))
                if nt is None:
                    notes.append(f"{s}:{r['symbol']}: element type `{r.get('element_type')}` not resolved")
                    continue
                base, dims = nt
                try:
                    es = m.size_align(base)[0]
                except KeyError:
                    notes.append(f"{s}:{r['symbol']}: type `{base}` not in the headers")
                    continue
                n = 1
                for d in dims:
                    n *= d
                if es * n != size:
                    if size % (es * n) == 0:
                        dims = [size // (es * n)] + dims
                    elif size % es == 0:
                        notes.append(f"{s}:{r['symbol']}: element_type `{r['element_type']}` x {r.get('count')} "
                                     f"!= size 0x{size:x}; mirrored as {base}[{size // es}]")
                        dims = [size // es]
                    else:
                        notes.append(f"{s}:{r['symbol']}: size 0x{size:x} is not a multiple of sizeof({base}) = {es}")
                        continue
                if r["symbol"] in out:
                    notes.append(f"{s}:{r['symbol']}: symbol also in stream {out[r['symbol']][2]}; first kept")
                    continue
                out[r["symbol"]] = (base, dims, s)
    return out, notes


# ------------------------------------------------------------------------------------------------ emit
RUNTIME = r'''

# ------------------------------------------------------------------------------------------------ runtime API
_TYPES = {n: globals()[n] for n in STRUCT_SIZES}


def _base(name):
    if name in _SCALARS:
        return getattr(ctypes, _SCALARS[name])
    if name in ARRAY_TYPEDEFS:
        b, dims = ARRAY_TYPEDEFS[name]
        return _shape(_base(b), dims)
    return _TYPES[name]


def _shape(t, dims):
    for d in reversed(dims):
        t = t * d
    return t


def ctype(symbol):
    """The ctypes type of a model symbol, `name` or `name[k:n]` (elements k..n-1 of an array symbol)."""
    m = re.fullmatch(r"(\w+)(?:\[(\d+):(\d+)\])?", symbol)
    if not m or m.group(1) not in SYMBOLS:
        raise KeyError(symbol)
    base, dims, _stream = SYMBOLS[m.group(1)]
    if m.group(2) is None:
        return _shape(_base(base), dims)
    k, n = int(m.group(2)), int(m.group(3))
    if not dims or not (0 <= k < n <= dims[0]):
        raise KeyError(symbol)
    return _shape(_base(base), [n - k] + list(dims[1:]))


def view(symbol, data):
    """bytes -> a ctypes object of the symbol's type (a copy; len(data) must equal its size)."""
    t = ctype(symbol)
    if len(data) != ctypes.sizeof(t):
        raise ValueError(f"{symbol}: {len(data)} bytes, type is {ctypes.sizeof(t)}")
    return t.from_buffer_copy(data)


def to_bytes(obj):
    return bytes(obj)


def parse(parts):
    """{symbol: bytes} -> {symbol: ctypes object}; keys the mirror cannot type (gaps, unnamed fields) stay bytes."""
    out = {}
    for k, v in parts.items():
        try:
            out[k] = view(k, v)
        except KeyError:
            out[k] = bytes(v)
    return out


def serialize(objs):
    """{symbol: ctypes object | bytes} -> {symbol: bytes}."""
    return {k: (bytes(v) if not isinstance(v, (bytes, bytearray)) else bytes(v)) for k, v in objs.items()}


def _is_char_array(t):
    return isinstance(t, type) and issubclass(t, ctypes.Array) and t._type_ is ctypes.c_char


def as_dict(obj):
    """A ctypes object -> plain Python values (structs as dicts, arrays as lists, char arrays as bytes)."""
    if isinstance(obj, ctypes.Structure):
        out = {}
        for f in obj._fields_:
            if _is_char_array(f[1]):     # ctypes returns a char[n] member cut at its first NUL: read it raw
                d = getattr(type(obj), f[0])
                out[f[0]] = ctypes.string_at(ctypes.addressof(obj) + d.offset, d.size)
            else:
                out[f[0]] = as_dict(getattr(obj, f[0]))
        return out
    if isinstance(obj, ctypes.Array):
        if obj._type_ is ctypes.c_char:
            return bytes(obj)
        return [as_dict(x) for x in obj]
    return obj


def _fill(obj, values):
    if isinstance(obj, ctypes.Structure):
        for f in obj._fields_:
            if _is_char_array(f[1]):
                d = getattr(type(obj), f[0])
                ctypes.memmove(ctypes.addressof(obj) + d.offset, bytes(values[f[0]]), d.size)
                continue
            cur = getattr(obj, f[0])
            if isinstance(cur, (ctypes.Structure, ctypes.Array)):
                _fill(cur, values[f[0]])
            else:
                setattr(obj, f[0], values[f[0]])
    elif obj._type_ is ctypes.c_char:
        ctypes.memmove(obj, bytes(values), len(values))
    elif issubclass(obj._type_, (ctypes.Structure, ctypes.Array)):
        for i, v in enumerate(values):
            _fill(obj[i], v)
    else:
        for i, v in enumerate(values):
            obj[i] = v


def from_values(symbol, values):
    """as_dict()'s output -> a NEW zero-filled object of the symbol's type with every member assigned. Bytes outside
    every member (struct padding) come back as 0, so bytes(from_values(s, as_dict(view(s, d)))) == d only when the
    type accounts for every byte of d."""
    t = ctype(symbol)
    obj = t()
    if isinstance(obj, (ctypes.Structure, ctypes.Array)):
        _fill(obj, values)
    else:
        obj.value = values
    return obj


def value_roundtrip(parts):
    """{symbol: bytes} -> ({symbol: bytes rebuilt from values}, [symbols whose bytes changed])."""
    out, bad = {}, []
    for k, v in parts.items():
        try:
            o = view(k, v)
        except KeyError:
            out[k] = bytes(v)
            continue
        val = o.value if not isinstance(o, (ctypes.Structure, ctypes.Array)) else as_dict(o)
        out[k] = bytes(from_values(k, val))
        if out[k] != bytes(v):
            bad.append(k)
    return out, bad


# ---- .sve files through the field table (re/model/sve_fields.tsv: order, sizes, tail conditions; its va column is
#      not read). A body part is keyed by its model symbol; a field with no model symbol by `field<index>`, an untyped
#      gap inside a composed field by `field<index>+0x<offset>`.
def load_field_table(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            size = int(r["size"], 16)
            m = re.match(r"flags & (0x[0-9a-fA-F]+) \(field (\d+)\)", r["present_if"])
            members = []
            if r["model_symbols"] != "-":
                for s in r["model_symbols"].split(";"):
                    sym, _, off = s.partition("@+")
                    members.append((sym, int(off, 16) if off else 0))
            rows.append({"index": r["index"], "part": r["part"], "size": size, "mapping": r["mapping"],
                         "members": members, "mask": int(m.group(1), 16) if m else None,
                         "flag_field": m.group(2) if m else None})
    return rows


def field_parts(row, data):
    """One field's bytes -> [(key, bytes)] covering it exactly, in order."""
    out, pos = [], 0
    for sym, off in row["members"]:
        try:
            n = ctypes.sizeof(ctype(sym))
        except KeyError:
            n = None
        if n is None:
            break
        if off > pos:
            out.append((f"field{row['index']}+0x{pos:x}", data[pos:off]))
        out.append((sym, data[off:off + n]))
        pos = off + n
    else:
        if pos < len(data):
            out.append((f"field{row['index']}+0x{pos:x}" if pos else f"field{row['index']}", data[pos:]))
        return out
    return [(f"field{row['index']}", data)]


def body_layout(table, flags):
    return [r for r in table if r["part"] not in ("header", "header2") and (r["mask"] is None or flags & r["mask"])]


def split_sve(blob, table):
    """A .sve file -> (headers [bytes], {key: bytes}). One header, or two (championship) when the size says so."""
    hdr = next(r["size"] for r in table if r["part"] == "header")
    flags_idx = next(r["flag_field"] for r in table if r["flag_field"])
    for nh in (1, 2):
        pos, parts, flags = hdr * nh, {}, None
        body = [r for r in table if r["part"] == "body"]
        rows = list(body)
        i = 0
        ok = True
        while i < len(rows):
            r = rows[i]
            if pos + r["size"] > len(blob):
                ok = False
                break
            data = blob[pos:pos + r["size"]]
            for k, v in field_parts(r, data):
                parts[k] = v
            if r["index"] == flags_idx:
                flags = int.from_bytes(data[:4], "little")
                rows = body_layout(table, flags)
            pos += r["size"]
            i += 1
        if ok and pos == len(blob):
            return [blob[k * hdr:(k + 1) * hdr] for k in range(nh)], parts
    raise ValueError(f"{len(blob)} bytes do not fit the field table with one or two headers")


def join_sve(headers, parts, table):
    flags_idx = next(r["flag_field"] for r in table if r["flag_field"])
    flags_row = next(r for r in table if r["index"] == flags_idx)
    fkey = field_parts(flags_row, b"\0" * flags_row["size"])[0][0]
    flags = int.from_bytes(bytes(parts[fkey])[:4], "little")
    out = [bytes(h) for h in headers]
    for r in body_layout(table, flags):
        for k, _v in field_parts(r, b"\0" * r["size"]):
            out.append(bytes(parts[k]))
    return b"".join(out)


def dump_layout(table):
    """The checkpoint dump of re/frida/scenario_ab.py: every body field and every tail block, in table order, then
    8 bytes of extra words (RNG seed, seed time)."""
    return [r for r in table if r["part"] not in ("header", "header2")]


def split_dump(blob, table, extra=8):
    parts, pos = {}, 0
    for r in dump_layout(table):
        for k, v in field_parts(r, blob[pos:pos + r["size"]]):
            parts[k] = v
        pos += r["size"]
    if pos + extra != len(blob):
        raise ValueError(f"dump is {len(blob)} bytes, the table gives {pos} + {extra}")
    return parts
'''


def emit(m: Model, syms: dict) -> str:
    used = set()

    def need(t):
        t = ALIASES.get(t, t)
        if t in SCALARS or t in used:
            return
        if t in m.arrays:
            need(m.arrays[t][0])
            used.add(t)
            return
        for _f, b, _d, _c in m.structs[t]["fields"]:
            need(b)
        used.add(t)
        order.append(t)
    order: list[str] = []
    for s in m.order:
        need(s)
    for b, _d, _s in syms.values():
        need(b)

    def ctexpr(base, dims):
        b = ALIASES.get(base, base)
        if b in SCALARS:
            e = f"ctypes.{SCALARS[b][0]}"
        elif b in m.arrays:
            ab, ad = m.arrays[b]
            return ctexpr(ab, list(dims) + list(ad))
        else:
            e = b
        for d in reversed(dims):
            e = f"({e} * {d})"
        return e
    lines = ['"""GENERATED by re/tools/model_ctypes_gen.py from re/model/include/sections/*.h and the streams\' address maps.',
             "Do not edit: rerun the generator. Python ctypes mirror of the phase-1 data model (stream G2). No original",
             "address is embedded: SYMBOLS maps model symbol names to (element type, shape, stream); the .sve helpers read",
             "re/model/sve_fields.tsv for field order and sizes.",
             "",
             "    import model_ctypes as M",
             "    g = M.view('g_golfers', data)            # Golfer * 156",
             "    g[3].mood, M.as_dict(g[3])['hole_strokes']",
             "    table = M.load_field_table('re/model/sve_fields.tsv')",
             "    headers, parts = M.split_sve(blob, table)   # {symbol: bytes}",
             "    assert M.join_sve(headers, M.serialize(M.parse(parts)), table) == blob",
             '"""', "import csv", "import ctypes", "import re", ""]
    sizes = {}
    for s in order:
        if s in m.arrays:
            continue
        lay = m.compute(s)
        sizes[s] = lay["size"]
        lines.append(f"class {s}(ctypes.Structure):  # {m.structs[s]['file']}, 0x{lay['size']:x} bytes")
        fl = [f"    (\"{fn}\", {ctexpr(b, d)})," for fn, b, d, _c in m.structs[s]["fields"]]
        lines.append("    _fields_ = [")
        lines += ["    " + x for x in fl]
        lines.append("    ]")
        lines.append("")
        lines.append("")
    lines.append("STRUCT_SIZES = {")
    lines += [f"    \"{s}\": 0x{v:x}," for s, v in sizes.items()]
    lines.append("}")
    lines.append("STRUCT_OFFSETS = {")
    for s in sizes:
        offs = m.compute(s)["offsets"]
        lines.append(f"    \"{s}\": {{" + ", ".join(f"\"{k}\": 0x{v:x}" for k, v in offs.items()) + "},")
    lines.append("}")
    lines.append("for _n, _s in STRUCT_SIZES.items():")
    lines.append("    assert ctypes.sizeof(globals()[_n]) == _s, (_n, ctypes.sizeof(globals()[_n]), _s)")
    lines.append("    for _f, _o in STRUCT_OFFSETS[_n].items():")
    lines.append("        assert getattr(globals()[_n], _f).offset == _o, (_n, _f)")
    lines.append("")
    lines.append("_SCALARS = {" + ", ".join(f"\"{k}\": \"{v[0]}\"" for k, v in SCALARS.items()) + "}")
    lines.append("ARRAY_TYPEDEFS = {")
    lines += [f"    \"{k}\": (\"{b}\", {list(d)})," for k, (b, d) in m.arrays.items()]
    lines.append("}")
    lines.append("SYMBOLS = {  # model symbol -> (element type, shape, stream)")
    lines += [f"    \"{k}\": (\"{b}\", {list(d)}, \"{s}\")," for k, (b, d, s) in sorted(syms.items())]
    lines.append("}")
    return "\n".join(lines) + RUNTIME


def build() -> tuple[str, Model, dict, list[str], list[str]]:
    m = Model()
    for p in sorted(SECTIONS.glob("*.h")):
        m.parse(p)
    errs = check_model(m)
    syms, notes = symbols(m)
    return emit(m, syms), m, syms, errs, notes


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="fail if re/model/model_ctypes.py differs from a fresh build")
    ap.add_argument("--notes", action="store_true", help="list every address-map row the mirror could not type")
    a = ap.parse_args()
    src, m, syms, errs, notes = build()
    print(f"structs {len(m.layout)}, array typedefs {len(m.arrays)}, static asserts {len(m.asserts)}, "
          f"offset comments {sum(1 for s in m.structs.values() for f in s['fields'] if f[3] is not None)}, "
          f"symbols typed {len(syms)}, rows not typed {len(notes)}")
    for e in errs:
        print("ERROR", e)
    if a.notes:
        for n in notes:
            print("note", n)
    if errs:
        return 1
    if a.check:
        ok = OUT.exists() and OUT.read_text(encoding="utf-8") == src
        print("model_ctypes.py " + ("current" if ok else "STALE"))
        return 0 if ok else 1
    OUT.write_text(src, encoding="utf-8", newline="\n")
    # import the result: its own asserts compare ctypes' layout with the computed one
    sys.path.insert(0, str(OUT.parent))
    import importlib
    mod = importlib.import_module("model_ctypes")
    for s in mod.SYMBOLS:
        ctypes.sizeof(mod.ctype(s))
    print(f"wrote {OUT.relative_to(ROOT)} ({len(src.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
