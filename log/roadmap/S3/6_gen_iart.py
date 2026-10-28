"""S3.6: loadInterfaceArt(1)'s whole image half for the graphics backend, generated into port/src/sim/gfx_hud.cpp between
`// BEGIN GENERATED loadInterfaceArt images` and `// END GENERATED loadInterfaceArt images`.

Source: the C3 hook source shim/src/re/c3fv.cpp, LoadInterfaceArt_re after its `full == 0` return, and its kIaN tables. Kept:
every image load and cut (kLoad / kBlit rows, Blit, LoadPcx, Row, Grid, TableRow, SetlRow, PairRows, WideRow, LoadHeads,
the frame Image's Surface_createFull and the global Surface_createFull) in the original's order, with the loops as written.
Dropped: the stores and fills (G<int>(..) = .., Fill32: the simulation's, menu.cpp runs them) and the kFlic / kSet rows
(loadFlic is flic.cpp's, S3.4). Every other body line must be one of the known forms, or the generator stops.

Every literal inside golf_clean.exe's .data becomes GSO(<GameState object>, <offset>) (an offset from the GameState's
start; the body adds the state's base), so the port file keeps no image address. Comments are dropped; each table row
keeps its call site as `loadInterfaceArt+0x<off>`.

    ~/bin/py -3.12 log/roadmap/S3/6_gen_iart.py [--check]       (--check: exit 1 when gfx_hud.cpp is not up to date)
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "log" / "p3"))
import p37a_conv as C  # noqa: E402

SRC = ROOT / "shim" / "src" / "re" / "c3fv.cpp"
OUT = ROOT / "port" / "src" / "sim" / "gfx_hud.cpp"
BEGIN = "// BEGIN GENERATED loadInterfaceArt images (log/roadmap/S3/6_gen_iart.py)"
END = "// END GENERATED loadInterfaceArt images"
FN = 0x00442180


def gso(va):
    o = C.obj(va)
    if o is None or not (C.DATA_LO <= va < C.DATA_HI):
        sys.exit("address %#x is in no GameState object" % va)
    return "GSO(%s, 0x%x)" % o


def lit(m):
    v = int(m.group(0), 16)
    if C.DATA_LO <= v < C.DATA_HI:
        return gso(v)
    if 0x00400000 <= v < 0x00900000:
        sys.exit("literal %s outside .data" % m.group(0))
    return m.group(0)


HEX = re.compile(r"0x[0-9A-Fa-f]{6,8}\b")


def conv(code):
    return HEX.sub(lit, code)


def tables(src):
    out = {}
    for m in re.finditer(r"const Op (kIa\d+)\[\] = \{(.*?)\n\};", src, re.S):
        rows = []
        for r in re.finditer(r"\{(k\w+), ([^}]*)\},\s*//\s*(0x[0-9a-fA-F]+)?", m.group(2)):
            args = [int(x.strip(), 16) for x in r.group(2).split(",") if x.strip()]
            rows.append((r.group(1), args, int(r.group(3), 16) if r.group(3) else None))
        out[m.group(1)] = rows
    return out


def cite(va):
    return "loadInterfaceArt+0x%x" % (va - FN) if va else ""


def gen_table(name, rows):
    lines = []
    for kind, a, va in rows:
        if kind == "kLoad":   # {path, palette out, ofs, count, flags}
            pal = gso(a[1]) if a[1] else "0"
            lines.append("    {ArtLoad, %s, %s, 0x%x, 0x%x, 0x%x, 0, 0},  // %s" % (gso(a[0]), pal, a[2], a[3], a[4], cite(va)))
        elif kind == "kBlit":  # {dest, x, y, w, h, kind, trim}
            lines.append("    {ArtCut, %s, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x},  // %s" % (gso(a[0]), *a[1:7], cite(va)))
        elif kind in ("kFlic", "kSet"):
            continue
        else:
            sys.exit("unknown op kind %s in %s" % (kind, name))
    if not lines:
        return None
    return "const ArtOp %s[] = {\n%s\n};" % (name, "\n".join(lines))


SKIP = re.compile(r"^(//.*|G<int>\(0x[0-9A-Fa-f]+\) = .*;.*|Fill32\(.*\);.*|kImageCtor\(&img, 0\);.*|kImageDtor\(&img, 0\);.*)$")


def gen_body(body, have):
    out = []
    for ln in body.splitlines():
        s = ln.strip()
        if not s:
            continue
        code = re.sub(r"\s*//.*$", "", s)                  # comments dropped (they cite original addresses)
        if not code:
            continue
        if SKIP.match(code) or SKIP.match(s):
            continue
        ind = ln[:len(ln) - len(ln.lstrip())]
        if code == "FrameImage img;":
            continue                                       # the Art object holds the frame Image
        m = re.match(r"kSurfaceCreateFull\(&img, 0, .*\);$", code)
        if m:
            out.append(ind + "A.reset_image();")
            continue
        m = re.match(r"kSurfaceCreateFull\(P\((0x[0-9A-Fa-f]+)\), 0, .*\);$", code)
        if m:
            out.append(ind + "A.erase(%s);" % gso(int(m.group(1), 16)))
            continue
        m = re.match(r"RUN\(&img, (kIa\d+)\);$", code)
        if m:
            if m.group(1) in have:
                out.append(ind + "A.run(%s, sizeof(%s) / sizeof(%s[0]));" % ((m.group(1),) * 3))
            continue
        m = re.match(r"(Blit|Row|Grid|TableRow|SetlRow|PairRows|WideRow|LoadPcx|LoadHeads)\((.*), &img(, .*)?\);$", code)
        if m:
            fn = {"Blit": "cut", "Row": "row", "Grid": "grid", "TableRow": "table_row", "SetlRow": "setl_row",
                  "PairRows": "pair_rows", "WideRow": "wide_row", "LoadPcx": "load_pcx", "LoadHeads": "heads"}[m.group(1)]
            out.append(ind + "A.%s(%s%s);" % (fn, conv(m.group(2)), conv(m.group(3) or "")))
            continue
        m = re.match(r"(LoadPcx|LoadHeads)\(&img, (.*)\);$", code)
        if m:
            fn = {"LoadPcx": "load_pcx", "LoadHeads": "heads"}[m.group(1)]
            out.append(ind + "A.%s(%s);" % (fn, conv(m.group(2))))
            continue
        if re.match(r"^(for \(.*\) \{|for \(.*\)|\{|\}|const int .*;|static const (unsigned|short) \w+\[\d+\] = \{.*|"
                    r"(?:0x[0-9A-Fa-f]+, )*0x[0-9A-Fa-f]+\};|unsigned d = .*;|int k = 0;|int i = 0;|d \+= .*;)$", code):
            code = code.replace("static const unsigned", "static const uint32_t")
            code = re.sub(r"\bunsigned\b", "uint32_t", code)
            out.append(ind + conv(code))
            continue
        sys.exit("unknown body line: %r" % s)
    return out


def main():
    src = SRC.read_text(encoding="utf-8")
    tabs = tables(src)
    body = src[src.index("void __cdecl LoadInterfaceArt_re(int full) {"):]
    body = body[:body.index("\n}\n")]
    start = body.index("if (full == 0) {")
    body = body[body.index("return;\n    }\n", start) + len("return;\n    }\n"):]
    gen_tabs, have = [], set()
    for name in sorted(tabs, key=lambda n: int(n[3:])):
        t = gen_table(name, tabs[name])
        if t:
            gen_tabs.append(t)
            have.add(name)
    lines = gen_body(body, have)
    block = [BEGIN, "// %d tables, %d body lines; regenerate after changing c3fv.cpp's LoadInterfaceArt_re" % (len(gen_tabs), len(lines))]
    block += gen_tabs
    block.append("void iart_images(Art &A) {")
    block += lines
    block.append("}")
    block.append(END)
    text = OUT.read_text(encoding="utf-8")
    i, j = text.index(BEGIN), text.index(END) + len(END)
    new = text[:i] + "\n".join(block) + text[j:]
    if "--check" in sys.argv:
        print("up to date" if new == text else "OUT OF DATE")
        return 0 if new == text else 1
    OUT.write_text(new, encoding="utf-8", newline="\n")
    print("wrote %d tables, %d body lines" % (len(gen_tabs), len(lines)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
