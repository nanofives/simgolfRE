"""Static check of proposed data-model types against every access in golf_clean.exe.

    py -3.12 re/tools/model_check.py <types.tsv> [--out <violations.tsv>] [--margin 0x100] [--all-functions]

<types.tsv> (tab-separated, header row; one typed range per row):
    module      golf_clean.exe (the only module the scanner reads)
    va          start address (hex)
    size        bytes (hex or decimal); empty = count * stride
    type        element type: char uchar short ushort int uint ptr float double flags8 flags16 flags32 struct
    count       number of elements
    stride      bytes per element (empty = the element type's size)
    sign        s / u / empty (default from the type: char short int signed; uchar ushort uint ptr flags unsigned)
    const       1 = no instruction may write the range
    fields      struct only: `off:type[n],off:type,...` (offsets hex, n an array length) or
                `cstruct:<path>#<Name>` = the `typedef struct <Name> { ... }` block of that file (int8_t .. uint32_t,
                float, double and pointer members, `/* +0xNN */` offset comments are checked against the computed
                layout)
    name        label for the report

Every reference re/tools/model_scan.py finds whose address lies in [va - margin, va + size + margin) is checked
(functions of the whole module with --all-functions, else game logic 0x401000..0x4a4ecf, mainLoop's whole body
included; b1 left no code outside a function range). Violations (column `violation`):
    width_cross     the access runs past the end of the field (or element) it starts in
    width_block     (warning) an access of several elements of an array field at once (an inline block copy)
    width_narrow    (warning) a narrower access to a field that is not a flags type (a byte test of a word)
    float_int       an x87 float operation on an integer field, or an integer access of a float / double field
    unaligned       the access does not start at a multiple of its width inside the field element
    no_field        a struct access at an offset no field covers
    stride          (warning) an indexed access whose stride (from tracked multipliers, not loop steps) is not the
                    element stride, a multiple of it, or the element size of the field array it lands in
    index_bound     an indexed access whose constant bound (`cmp r, N` + ja / jae / jg / jge before it) lets the
                    index run past the range
    loop_end        a loop-end compare (`cmp reg, imm` + jcc) of a pointer loaded with an address inside the range,
                    whose last element lies past the range
    base_outside    (warning) an indexed access with the range's stride whose displacement lies outside the range
                    (a biased index such as record i - 1, or a wrong base: the golfer 0x5794c0 / 0x5794b8 case)
    sign            (warning) the access's signedness (movsx / movzx, or the jcc after a compare) disagrees
    const_write     a write into a range declared const
Address-of uses (lea, immediates) and string instructions (rep movs / stos, scas: whole-block copies and clears)
are checked only for range, not for width.
"""
import argparse, collections, csv, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import model_scan as ms  # noqa: E402

# errors: the type and the instruction cannot both be right. warnings: read the instruction (byte tests of a word,
# multi-element copies, signedness, strides from straight-line register tracking, biased indexes).
SEVERITY = {"no_field": "error", "unaligned": "error", "width_cross": "error", "float_int": "error",
            "const_write": "error", "index_bound": "error", "loop_end": "error",
            "width_narrow": "warning", "width_block": "warning", "sign": "warning", "stride": "warning",
            "base_outside": "warning"}
LOOP_JCC = {"jl", "jb", "jne", "jle", "jbe", "jg", "ja", "jge", "jae"}
TYPES = {  # name -> (size, default sign, kind)
    "char": (1, "s", "int"), "int8_t": (1, "s", "int"), "uchar": (1, "u", "int"), "uint8_t": (1, "u", "int"),
    "short": (2, "s", "int"), "int16_t": (2, "s", "int"), "ushort": (2, "u", "int"), "uint16_t": (2, "u", "int"),
    "int": (4, "s", "int"), "int32_t": (4, "s", "int"), "uint": (4, "u", "int"), "uint32_t": (4, "u", "int"),
    "ptr": (4, "u", "ptr"), "float": (4, "", "float"), "double": (8, "", "float"),
    "flags8": (1, "", "flags"), "flags16": (2, "", "flags"), "flags32": (4, "", "flags"),
}


def num(s):
    s = (s or "").strip()
    return int(s, 16) if s.lower().startswith("0x") else int(s) if s else None


class Field:
    def __init__(self, off, typ, n=1, name="", sign=None):
        if typ.endswith("*"):
            typ = "ptr"
        size, dsign, kind = TYPES[typ]
        self.off, self.typ, self.n, self.size, self.kind, self.name = off, typ, n, size, kind, name
        self.sign = dsign if sign is None else sign

    @property
    def end(self):
        return self.off + self.size * self.n


def parse_fields(spec):
    """`off:type[n],...` -> [Field]"""
    out = []
    for part in [p.strip() for p in spec.split(",") if p.strip()]:
        m = re.fullmatch(r"(0x[0-9a-fA-F]+|\d+):([\w*]+)(?:\[(\d+|0x[0-9a-fA-F]+)\])?(?::(\w+))?", part)
        if not m:
            raise ValueError(f"bad field {part!r}")
        out.append(Field(num(m.group(1)), m.group(2), num(m.group(3)) if m.group(3) else 1, m.group(4) or ""))
    return out


def parse_cstruct(path, name):
    """members of `typedef struct <name> { ... }` in a markdown / header file, offsets computed with natural
    alignment; a `/* +0xNN` comment that disagrees with the computed offset is an error"""
    text = (ROOT / path).read_text(encoding="utf-8")
    m = re.search(r"typedef\s+struct\s+" + re.escape(name) + r"\s*\{(.*?)\}\s*" + re.escape(name), text, re.S)
    if not m:
        raise ValueError(f"struct {name} not found in {path}")
    out, off = [], 0
    for line in m.group(1).splitlines():
        mm = re.match(r"\s*(\w+)\s*(\*?)\s*(\w+)\s*(?:\[(\w+)\])?\s*;(?:\s*/\*\s*\+(0x[0-9a-fA-F]+))?", line)
        if not mm:
            continue
        typ = "ptr" if mm.group(2) else mm.group(1)
        if typ not in TYPES:
            raise ValueError(f"{name}: type {typ} not supported")
        n = num(mm.group(4)) if mm.group(4) else 1
        size = TYPES[typ][0]
        off = (off + size - 1) // size * size
        if mm.group(5) and num(mm.group(5)) != off:
            raise ValueError(f"{name}.{mm.group(3)}: comment says +{mm.group(5)}, layout gives +{off:#x}")
        out.append(Field(off, typ, n, mm.group(3)))
        off += size * n
    return out


class Range:
    def __init__(self, row):
        self.module = row.get("module") or ms.MOD
        self.va = num(row["va"])
        self.typ = row["type"].strip()
        self.count = num(row.get("count")) or 1
        if self.typ == "struct":
            spec = (row.get("fields") or "").strip()
            if spec.startswith("cstruct:"):
                path, _, sname = spec[len("cstruct:"):].partition("#")
                self.fields = parse_cstruct(path, sname)
            else:
                self.fields = parse_fields(spec)
            esize = max((f.end for f in self.fields), default=0)
        else:
            self.fields = [Field(0, self.typ, 1, sign=(row.get("sign") or "").strip() or None)]
            esize = self.fields[0].size
        self.stride = num(row.get("stride")) or esize
        self.size = num(row.get("size")) or self.stride * self.count
        self.const = (row.get("const") or "").strip() in ("1", "y", "yes", "true")
        self.name = (row.get("name") or "").strip() or f"{self.typ}@{self.va:#x}"
        sign = (row.get("sign") or "").strip()
        if sign and self.typ == "struct":
            for f in self.fields:
                f.sign = sign

    def field_at(self, off):
        for f in self.fields:
            if f.off <= off < f.end:
                return f
        return None

    def allowed_strides(self, f):
        s = {self.stride}
        if f is not None and f.n > 1:
            s.add(f.size)
        return s


def check_range(rg, refs, margin):
    out = []
    lo, hi = rg.va, rg.va + rg.size
    for r in refs:
        if r.kind == "ptruse" or not (lo - margin <= r.addr < hi + margin):
            continue
        indexed = bool(r.base or r.index) and r.kind != "imm"
        strides = {s for s, src in r.strides if s and s > 0}
        v = []

        def add(kind, detail):
            v.append((kind, detail))

        strong = {s for s, src in r.strides if s and s > 0 and src != "step"}

        off = r.addr - lo
        inside = 0 <= off < rg.size
        if r.kind == "imm" and r.mnem == "cmp":
            # a loop end of THIS range: the compared pointer was loaded with an address inside the range and the
            # compare is followed by a conditional jump. With jl / jb / jne the last element is one stride below.
            if "cmpend" in r.evid and r.dest is not None and lo <= r.dest < hi and r.jcc in LOOP_JCC:
                last = r.addr - (0 if r.jcc in ("jle", "jbe") else max(strides | {rg.stride}))
                if last >= hi:
                    add("loop_end", f"loop from {r.dest:#x}: compare with {r.addr:#x} ({r.jcc}) reaches {last:#x}"
                        f" >= end {hi:#x}")
            inside = False                       # a bound, not an access
        elif not inside:
            # a neighbour's access unless it is indexed with this range's record stride (above 8): a biased index
            # or a wrong base. Small strides are left out: neighbouring scalar arrays share them.
            if indexed and rg.stride > 8 and rg.stride in strong:
                add("base_outside", f"displacement {r.addr:#x} lies {('-' if off < 0 else '+')}"
                    f"{abs(off if off < 0 else off - rg.size):#x} outside, stride {rg.stride:#x}")
        else:
            rec, eo = divmod(off, rg.stride)
            f = rg.field_at(eo)
            w = r.width or 0
            addr_only = r.kind == "imm" or r.kind.startswith("lea") or r.kind == "reg" and r.rep
            if rec >= rg.count:
                add("index_bound", f"fixed access to element {rec} of {rg.count}")
            if f is None and not addr_only:
                add("no_field", f"offset +{eo:#x} of element {rec}")
            elif f is not None and not addr_only and w:
                fo = (eo - f.off) % f.size
                if fo + w > f.size or w > f.size:
                    arr = f"{f.typ}{'[%d]' % f.n if f.n > 1 else ''} {f.name} (+{f.off:#x})"
                    if f.n > 1 and (eo - f.off) + w <= f.size * f.n and f.kind != "float" and fo == 0:
                        add("width_block", f"{w}-byte access at +{eo:#x} inside {arr}: several elements at once")
                    else:
                        add("width_cross", f"{w}-byte access at +{eo:#x} into {arr}")
                elif w < f.size and f.kind != "flags":
                    add("width_narrow", f"{w}-byte access at +{eo:#x} into {f.typ} {f.name} (+{f.off:#x})")
                if w and fo % min(w, f.size):
                    add("unaligned", f"{w}-byte access at +{eo:#x}, {fo} bytes into a {f.size}-byte element")
                if r.x87 and f.kind != "float":
                    add("float_int", f"x87 {r.mnem} on {f.typ} {f.name}")
                elif not r.x87 and f.kind == "float" and r.mnem not in ("mov", "push") and not r.rep:
                    add("float_int", f"integer {r.mnem} on {f.typ} {f.name}")
                if r.sign and f.sign and r.sign != f.sign and f.kind in ("int",):
                    add("sign", f"{'signed' if r.sign == 's' else 'unsigned'} use ({r.text}) of {f.typ} {f.name}")
            if indexed and strong and not r.rep:
                ok_strides = rg.allowed_strides(f)
                bad = [s for s in strong if not any(s == a or (a and s % a == 0 and s >= rg.stride) for a in ok_strides)]
                if bad and len(bad) == len(strong):
                    add("stride", f"stride {','.join(hex(s) for s in sorted(bad))} against element {rg.stride:#x}"
                        + (f" / field {f.size}" if f is not None and f.n > 1 else ""))
            if r.bound is not None and r.addr + r.bound >= hi:
                add("index_bound", f"index bound {r.bound:#x} bytes from {r.addr:#x} reaches past {hi:#x}")
            if rg.const and "w" in (r.acc or ""):
                add("const_write", r.text)
        for kind, detail in v:
            rec, eo = divmod(r.addr - lo, rg.stride) if 0 <= r.addr - lo < rg.size else (None, None)
            out.append(dict(range=rg.name, ins=r.ins, fn=r.fn, addr=r.addr, rec=rec, off=eo, ref_kind=r.kind,
                            text=r.text, violation=kind, severity=SEVERITY[kind], detail=detail))
    return out


def load_ranges(path):
    with open(path, encoding="utf-8") as fh:
        rows = [r for r in csv.DictReader((l for l in fh if not l.startswith("#")), delimiter="\t")]
    return [Range(r) for r in rows]


def scan_refs(all_functions=False):
    """every data reference of the module's functions (one scan, cached per process by model_scan.scan_cached)"""
    return ms.scan_cached(*((0, 1 << 32) if all_functions else ms.LOGIC))[1]


def run(ranges, margin=0x100, all_functions=False, names=None):
    out = []
    refs = scan_refs(all_functions)
    for rg in ranges:
        res = check_range(rg, refs, margin)
        if names:
            for v in res:
                v["fname"] = names.get(v["fn"], "")
        out += res
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("types")
    ap.add_argument("--out")
    ap.add_argument("--margin", type=lambda s: int(s, 0), default=0x100)
    ap.add_argument("--all-functions", action="store_true")
    a = ap.parse_args()
    import xref
    nm = {va: n for va, n, *_ in xref.functions(ms.MOD)}
    res = run(load_ranges(a.types), a.margin, a.all_functions, nm)
    cnt = collections.Counter((v["range"], v["severity"], v["violation"]) for v in res)
    for (rg, sev, k), n in sorted(cnt.items()):
        print(f"{rg}\t{sev}\t{k}\t{n}")
    sev = collections.Counter(v["severity"] for v in res)
    print(f"# {sev.get('error', 0)} errors, {sev.get('warning', 0)} warnings")
    if a.out:
        p = pathlib.Path(a.out)
        p.parent.mkdir(parents=True, exist_ok=True)
        with p.open("w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh, delimiter="\t", lineterminator="\n")
            w.writerow(["range", "severity", "violation", "ins", "function", "name", "addr", "record", "offset",
                        "ref_kind", "text", "detail"])
            for v in sorted(res, key=lambda v: (v["range"], v["severity"], v["violation"], v["ins"])):
                w.writerow([v["range"], v["severity"], v["violation"], f"0x{v['ins']:08x}", f"0x{v['fn']:08x}", v.get("fname", ""),
                            f"0x{v['addr']:08x}", "" if v["rec"] is None else v["rec"],
                            "" if v["off"] is None else hex(v["off"]), v["ref_kind"], v["text"], v["detail"]])


if __name__ == "__main__":
    main()
