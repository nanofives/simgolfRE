"""SimGolf .sve save files: parse, verify, diff, rewrite.

Format, from golf_clean.exe (analysis note: re/analysis/save/0040afa0_save_serializer.md):
  save  FUN_0040b4a0(name): open "saved games\\"+name+".sve" (flags 0x8301, 0x80), write the 100-byte
        text buffer DAT_0051a068 whole (the NUL-terminated "-<course>, <day> <month year>" plus whatever
        the buffer held after the NUL), then FUN_0040afa0(0).
  load  FUN_0040b9b0(name, ., ., champ): open (flags 0x8000), read 100 bytes into the same buffer, a second
        100 bytes when champ != 0 (Themes\\Championship\\ files), then FUN_0040afa0(1).
  body  FUN_0040afa0(mode) stores mode in DAT_0053e638 and calls FUN_0040af70(addr, size) for a fixed list
        of globals; FUN_0040af70 reads when DAT_0053e638 != 0 and writes otherwise. No compression, no
        checksum, no version field. Two tail blocks depend on DAT_0059e7b8, which is itself serialized
        earlier in the list (FIELDS below, in file order).

Fields are named g_<VA> (the global they restore) until a global gets a name in the master project.

Usage:
  py -3.12 re/tools/sve.py info  <file.sve>
  py -3.12 re/tools/sve.py diff  <a.sve> <b.sve>            # which globals differ, byte counts
  py -3.12 re/tools/sve.py dump  <file.sve> <g_XXXXXXXX|VA> [--dwords]
  py -3.12 re/tools/sve.py check <file.sve>...               # parse + byte-exact round trip
"""
import argparse, dataclasses, struct, sys

HEADER_LEN = 100              # FUN_0040b4a0 writes 100 (0x64) bytes of DAT_0051a068
FLAGS_VA = 0x0059E7B8         # DAT_0059e7b8, gates the two tail blocks
TAIL_A = (0x40000000, [(0x004E06C8, 0x8C0)])
TAIL_B = (0x00200000, [(0x0058F338, 0xB7C0), (0x00520A28, 0x19A28)])

# (VA, size) in the order FUN_0040afa0 serializes them (0x0040afa0..0x0040b492).
FIELDS = [
    (0x00822C78, 4), (0x005685F0, 4), (0x0058D36C, 4), (0x0059AAFC, 4), (0x005A34E0, 1),
    (0x0059AE78, 4), (0x00541CD8, 4), (0x005A882C, 4), (0x0056949C, 4), (0x005A636C, 4),
    (0x005A9CAC, 4), (0x005A9CB0, 4), (0x005787CC, 4), (0x00561258, 4), (0x00571FD4, 4),
    (0x0059BF90, 4), (0x00575AB0, 0x28A0), (0x005722E8, 0x9C4), (0x0056988C, 0x9C4),
    (0x0053CAF0, 5000), (0x0053BBAC, 0x9C4), (0x005A4998, 0xA29), (0x00838C1C, 0x169),
    (0x0058BCB8, 0x1000), (0x004C284C, 4), (0x00561250, 4), (0x005794B8, 0x9C00),
    (0x00585850, 0x1300), (0x00834170, 4), (0x005A6364, 4), (0x0053A450, 4), (0x00822C88, 4),
    (0x005409AC, 1000), (0x0051B388, 1000), (0x0053E63C, 1000), (0x004C2850, 4), (0x0059B730, 4),
    (0x0059AE7C, 4), (0x0059DEA0, 0x80), (0x0059C08C, 4), (0x00568600, 1000), (0x00822C70, 4),
    (0x005685F8, 4), (0x005A5A24, 4), (0x00584210, 2000), (0x00543CFC, 4), (0x004C2E14, 4),
    (0x00572CAC, 4), (0x0059AAF8, 4), (0x0056AE70, 0x1700), (0x005422F8, 0xF0),
    (0x0059AE80, 0x1C8), (0x0059B048, 4), (0x005736B0, 0x2400), (0x0059FC60, 0x3880),
    (0x005849E0, 0xE70), (0x005A59F8, 4), (0x005A5A04, 0x20), (0x0056D1B8, 6000),
    (0x004C2C94, 0x12), (0x005689E8, 800), (0x005419D0, 0x40), (0x00572CB0, 0xA00),
    (0x004D6088, 0xA640), (0x00543D10, 0x19A28), (0x00567B04, 4), (0x005A6378, 0x9C4),
    (0x0056C7E4, 0x9C4), (0x0053EA24, 0x9C4), (0x00542414, 0x9C4), (0x0059E7B8, 4),
    (0x004C1578, 0x420), (0x00571FD8, 0x30E), (0x00520640, 1000), (0x00567328, 100),
    (0x00571D38, 4), (0x0056FCB0, 0x1002), (0x0059D81C, 0x100), (0x005A46B8, 0x100),
    (0x0056A51C, 4), (0x005A6374, 4), (0x0056A524, 0x28), (0x005A47E0, 4), (0x005830B8, 0x9C4),
    (0x0059C090, 0x9C4), (0x005A5A00, 4),
]
# Names the master project or an analysis note already gives a global. Only add cited ones.
# "srf:" = FUN_00431fa0, the .srf course-summary writer ("wt", "[Course]\n"), field label = its fprintf format
# (strings 0x004c7844..0x004c7918); values cross-checked against tests/fixtures/championship/*.srf and the
# Load Previous Game panel for stories5.sve (re/analysis/save/0040afa0_save_serializer.md).
NAMES = {
    0x00834170: "date",   # FUN_0040b4a0 formats the header day from (v & 0x3ff)*30/1024+1 and month/year via FUN_0040d7b0(v)
    0x0059E7B8: "flags",  # gates the tail blocks (bits 0x40000000, 0x00200000)
    0x004C1578: "challenge_text",  # initial data is the string "1st Challenge hole..." (s_1st_Challenge_hole_004c1578)
    0x005685F0: "holes_plus1",     # srf: "Holes=%d" prints v - 1
    0x0059AAFC: "par",             # srf: "Par=%d"
    0x0058D36C: "yards",           # srf: "Yards=%d"
    0x005A34E0: "course_type",     # srf: "Type=%s" -> PTR_DAT_004c3078[v & 3]: 0 Park, 1 Desert, 2 Tropical, 3 Links
    0x00567328: "theme",           # srf: "Theme=%s" (char[100])
    0x0056A524: "records",         # srf: "Record=%d by %s" prints dword [0] (10 dwords)
    0x00571FD4: "cash_div100",     # srf: "Cash=%d" prints v * 100
    0x0059AE78: "fun_rating",      # srf: "FunRating=%d"
    0x00541CD8: "skill_rating_x100",     # srf: "SkillRating=%.2f" prints v * 0.01 (DAT_004ba488 = 0x3f847ae147ae147b)
    0x005A882C: "length_skill_x100",     # srf: "LengthSkill=%.2f", v * 0.01
    0x0056949C: "accuracy_skill_x100",   # srf: "AccuracySkill=%.2f", v * 0.01
    0x005A636C: "imagination_x100",      # srf: "Imagination=%.2f", v * 0.01
    0x005722E8: "tile_type",       # signed char [50][50], re/match/golf_small.cpp (100% match)
    0x0056988C: "tile_byte",       # unsigned char [50][50], re/match/golf_small.cpp (100% match)
    0x005794B8: "golfers",         # 0x9c records of 0x100 bytes, re/match/golf_story.cpp, golf_util.cpp; UNCERTAINTIES U-0003
    0x004D6088: "golfer_types",    # region holding the 0x230-stride records read at 0x4d60a8 + type*0x230 (golf_util.cpp 0x0046c940)
    0x0058BCB8: "placed_objects",  # 256 x 0x10, re/match/wip/golf_objects.cpp (0x00407000, not matched yet)
}
COURSE_TYPES = ["Park", "Desert", "Tropical", "Links"]  # PTR_DAT_004c3078: 0x4c30a4, 0x4c309c, 0x4c3090, 0x4c3088

# PTR_s_March_004c2908: 8 month-name pointers, index (date >> 10) & 7 (FUN_0040d7b0).
MONTHS = ["March", "April", "May", "June", "July", "August", "September", "October"]

def decode_date(v):
    """date global DAT_00834170 -> (day, month, year), as FUN_0040b4a0 / FUN_0040d7b0 format it:
    day = ((v & 0x3ff) * 0x1e >> 10) + 1, month = MONTHS[(v >> 10) & 7], year = (v >> 13) + 0x7d1."""
    return ((v & 0x3FF) * 30 >> 10) + 1, MONTHS[(v >> 10) & 7], (v >> 13) + 2001

@dataclasses.dataclass
class Field:
    va: int
    size: int
    offset: int
    data: bytes
    @property
    def name(self): return NAMES.get(self.va, f"g_{self.va:08X}")
    def u32(self): return struct.unpack_from("<I", self.data)[0] if self.size == 4 else None

@dataclasses.dataclass
class Save:
    headers: list            # 1 (normal) or 2 (championship) raw 100-byte header buffers
    fields: list
    @property
    def title(self): return self.headers[0].split(b"\0", 1)[0].decode("latin-1")
    def field(self, key):
        va = key
        if isinstance(key, str):
            va = int(key[2:], 16) if key.startswith("g_") else int(key, 16) if key[:2] in ("0x", "0X") else None
        for f in self.fields:
            if f.va == va or f.name == key: return f
        raise KeyError(key)
    def to_bytes(self):
        return b"".join(self.headers) + b"".join(f.data for f in self.fields)

def layout(flags):
    out = list(FIELDS)
    for bit, extra in (TAIL_A, TAIL_B):
        if flags & bit: out += extra
    return out

def parse(blob, headers=None):
    """headers=None: decide 1 or 2 header buffers from the file size (the body length is fixed by flags)."""
    for nh in ([headers] if headers else [1, 2]):
        off = HEADER_LEN * nh
        fields, flags = [], None
        spec = list(FIELDS)
        i = 0
        while i < len(spec):
            va, size = spec[i]
            if off + size > len(blob): break
            f = Field(va, size, off, blob[off:off + size]); fields.append(f); off += size
            if va == FLAGS_VA:
                flags = f.u32(); spec = layout(flags)
            i += 1
        if i == len(spec) and off == len(blob):
            return Save([blob[k * HEADER_LEN:(k + 1) * HEADER_LEN] for k in range(nh)], fields)
    raise ValueError(f"size {len(blob)} does not match the FUN_0040afa0 layout with 1 or 2 headers")

def load(path): return parse(open(path, "rb").read())

def summary(s):
    """The values FUN_00431fa0 writes to a .srf file, decoded the same way."""
    i = lambda va: struct.unpack("<i", s.field(va).data[:4])[0]
    theme = s.field(0x00567328).data.split(b"\0", 1)[0].decode("latin-1")
    return (f"Holes={i(0x005685F0) - 1} Par={i(0x0059AAFC)} Yards={i(0x0058D36C)} "
            f"Type={COURSE_TYPES[s.field(0x005A34E0).data[0] & 3]} Theme={theme} Record={i(0x0056A524)} "
            f"Cash={i(0x00571FD4) * 100} FunRating={i(0x0059AE78)} SkillRating={i(0x00541CD8) * 0.01:.2f} "
            f"LengthSkill={i(0x005A882C) * 0.01:.2f} AccuracySkill={i(0x0056949C) * 0.01:.2f} "
            f"Imagination={i(0x005A636C) * 0.01:.2f}")

def cmd_info(a):
    s = load(a.file)
    flags = s.field(FLAGS_VA).u32()
    print(f"title   {s.title!r}  headers={len(s.headers)}  fields={len(s.fields)}  bytes={len(s.to_bytes())}")
    print("date    %d %s %d" % decode_date(s.field(0x00834170).u32()))
    print("course  " + summary(s))
    print(f"flags   0x{flags:08X}  tailA(0x40000000)={'on' if flags & TAIL_A[0] else 'off'}  tailB(0x00200000)={'on' if flags & TAIL_B[0] else 'off'}")
    for f in s.fields:
        nz = sum(1 for b in f.data if b)
        v = f" = 0x{f.u32():08X} ({struct.unpack('<i', f.data)[0]})" if f.size == 4 else f.data.hex() if f.size == 1 else f"  nonzero {nz}/{f.size}"
        print(f"  +0x{f.offset:05X}  {f.name:<16} 0x{f.size:<6X}{v}")

def cmd_diff(a):
    x, y = load(a.a), load(a.b)
    print(f"A {x.title!r}\nB {y.title!r}")
    for fx, fy in zip(x.fields, y.fields):
        if fx.data != fy.data:
            n = sum(1 for p, q in zip(fx.data, fy.data) if p != q)
            extra = f"  0x{fx.u32():08X} -> 0x{fy.u32():08X}" if fx.size == 4 else ""
            print(f"  {fx.name:<16} size 0x{fx.size:<6X} {n:6d} bytes differ{extra}")
    if len(x.fields) != len(y.fields): print(f"  tail blocks differ: {len(x.fields)} vs {len(y.fields)} fields")

def cmd_dump(a):
    f = load(a.file).field(a.field)
    if a.dwords:
        for i in range(0, f.size - f.size % 4, 4):
            v = struct.unpack_from("<I", f.data, i)[0]
            if v: print(f"  [{i // 4:5d}] +0x{i:05X}  0x{v:08X}  {struct.unpack_from('<i', f.data, i)[0]}")
    else:
        for i in range(0, f.size, 16):
            row = f.data[i:i + 16]
            if any(row): print(f"  +0x{i:05X}  {row.hex(' ')}  {''.join(chr(b) if 32 <= b < 127 else '.' for b in row)}")

def cmd_check(a):
    bad = 0
    for p in a.files:
        blob = open(p, "rb").read(); s = parse(blob)
        d, m, y = decode_date(s.field(0x00834170).u32())
        ok = s.to_bytes() == blob and s.title.endswith(f", {d} {m} {y}")  # header date == decoded body date
        bad += not ok
        print(f"{'OK ' if ok else 'BAD'} {p}  {s.title!r}  date={d} {m} {y}  fields={len(s.fields)}")
    return 1 if bad else 0

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("info"); p.add_argument("file"); p.set_defaults(fn=cmd_info)
    p = sp.add_parser("diff"); p.add_argument("a"); p.add_argument("b"); p.set_defaults(fn=cmd_diff)
    p = sp.add_parser("dump"); p.add_argument("file"); p.add_argument("field"); p.add_argument("--dwords", action="store_true"); p.set_defaults(fn=cmd_dump)
    p = sp.add_parser("check"); p.add_argument("files", nargs="+"); p.set_defaults(fn=cmd_check)
    a = ap.parse_args()
    return a.fn(a) or 0

if __name__ == "__main__":
    sys.exit(main())
