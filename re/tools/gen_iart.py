"""Phase 4 (the boot path): loadInterfaceArt(1)'s full arm as a port table -> port/src/sim/menu_iart.inc (included by menu.cpp).

Source: the C3 hook source shim/src/re/c3fv.cpp, LoadInterfaceArt_re after its `full == 0` return. The body is a sequence of
table resets (Fill32), single stores (G<int>(addr) = v), RUN(&img, kIaN) over `const Op kIaN[]` tables of {kind, args} rows
(kFlic = loadFlic(id, path, frames, delay, flag), kSet = a dword store, kLoad / kBlit = an image load or a cut), LoadHeads
(listMatchingFiles and a counter per golfer head), and drawing helpers (Blit, Row, Grid, TableRow, WideRow, SetlRow, PairRows,
LoadPcx, Pal, Surface / Image constructors) whose bodies only load images and blit them into global Surfaces (checked: they
store nothing else). Kept, in the original's order: every store, fill, loadFlic and LoadHeads. Every other body line must be
one of the known drawing forms, or the generator stops, so a change in the hook cannot drop a store silently.

Addresses are written as GSO(<GameState object>, <offset>) (a data offset; port code keeps no image address).

    ~/bin/py -3.12 re/tools/gen_iart.py [--check]
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "log" / "p3"))
import p37a_conv as C  # noqa: E402  (obj(): the GameState object holding a .data address)

SRC = ROOT / "shim" / "src" / "re" / "c3fv.cpp"
OUT = ROOT / "port" / "src" / "sim" / "menu_iart.inc"
# g_sprites570cb8 (96 SpriteRefs of 0x2c): the build palette's hit test reads their cut height (+0x1c), so cuts into it are kept
SPR_LO, SPR_HI = 0x00570CB8, 0x00570CB8 + 96 * 0x2C

SKIP = re.compile(r"^\s*(//.*|\{|\}|\{\s*//.*|FrameImage img;|kImageCtor\(&img, 0\);.*|kImageDtor\(&img, 0\);.*|"
                  r"kSurfaceCreateFull\(.*|(Blit|Row|Grid|TableRow|WideRow|SetlRow|PairRows|LoadPcx|Pal)\(.*|"
                  r"for \(.*|const int .*|static const .*|unsigned d = .*|int k = 0;|0x[0-9A-Fa-f, ]+\};?|"
                  r"\s*0x[0-9A-Fa-f, ]+.*\};?)\s*$")


def gso(va):
    o = C.obj(va)
    if o is None or not (C.DATA_LO <= va < C.DATA_HI):
        sys.exit("address %#x is in no GameState object" % va)
    return "GSO(%s, 0x%x)" % o


def tables(src):
    out = {}
    for m in re.finditer(r"const Op (kIa\d+)\[\] = \{(.*?)\n\};", src, re.S):
        rows = []
        for r in re.finditer(r"\{(k\w+), ([^}]*)\},\s*//\s*(0x[0-9a-fA-F]+)?", m.group(2)):
            args = [int(x.strip(), 16) for x in r.group(2).split(",") if x.strip()]
            rows.append((r.group(1), args, r.group(3) or ""))
        out[m.group(1)] = rows
    return out


def cite(va):
    return "loadInterfaceArt+0x%x" % (int(va, 16) - 0x00442180) if va else ""


def main():
    src = SRC.read_text(encoding="utf-8")
    tabs = tables(src)
    body = src[src.index("void __cdecl LoadInterfaceArt_re(int full) {"):]
    body = body[:body.index("\n}\n")]
    start = body.index("if (full == 0) {")
    body = body[body.index("return;\n    }\n", start) + len("return;\n    }\n"):]
    steps = []
    for ln in body.splitlines():
        s = ln.strip()
        if not s:
            continue
        m = re.match(r"G<int>\((0x[0-9A-Fa-f]+)\) = (-?(?:0x[0-9A-Fa-f]+|\d+));\s*//\s*(0x[0-9a-fA-F]+)", s)
        if m:
            steps.append(("IaSet", gso(int(m.group(1), 16)), int(m.group(2), 0) & 0xFFFFFFFF, 0, 0, 0, 0, cite(m.group(3))))
            continue
        m = re.match(r"Fill32\((0x[0-9A-Fa-f]+), (0x[0-9A-Fa-f]+), (0x[0-9A-Fa-f]+|0)\);", s)
        if m:
            steps.append(("IaFill", gso(int(m.group(1), 16)), int(m.group(2), 16), int(m.group(3), 16), 0, 0, 0, ""))
            continue
        m = re.match(r"RUN\(&img, (kIa\d+)\);", s)
        if m:
            for kind, a, va in tabs[m.group(1)]:
                if kind == "kFlic":
                    delay = a[3] - (1 << 32) if a[3] >= 1 << 31 else a[3]
                    steps.append(("IaFlic", a[0], gso(a[1]), 0, a[2], delay, a[4], cite(va)))
                elif kind == "kSet":
                    steps.append(("IaSet", gso(a[0]), a[1], 0, 0, 0, 0, cite(va)))
                elif kind == "kBlit" and SPR_LO <= a[0] < SPR_HI:
                    # cropSprite into a palette SpriteRef: w, h and kind are stored before any pixel is read (c3ee.cpp)
                    steps.append(("IaCut", gso(a[0]), a[3], a[4], 0, 0, a[5], cite(va)))
                elif kind not in ("kLoad", "kBlit"):
                    sys.exit("unknown op %s in %s" % (kind, m.group(1)))
            continue
        m = re.match(r"LoadHeads\(&img, (0x[0-9A-Fa-f]+), '(.)', '(.)', (0x[0-9A-Fa-f]+)\);\s*//\s*(0x[0-9a-fA-F]+)", s)
        if m:
            steps.append(("IaHeads", gso(int(m.group(1), 16)), ord(m.group(2)) | ord(m.group(3)) << 8, gso(int(m.group(4), 16)),
                          0, 0, 0, cite(m.group(5))))
            continue
        if SKIP.match(ln):
            m = re.match(r"\s*(?:Blit|Row|Grid|TableRow|WideRow|SetlRow|PairRows)\((0x[0-9A-Fa-f]+)", ln)
            if m and SPR_LO <= int(m.group(1), 16) < SPR_HI:
                sys.exit("a drawing helper cuts into g_sprites570cb8: %r" % ln)
            continue
        sys.exit("unknown body line: %r" % ln)
    nf = sum(1 for x in steps if x[0] == "IaFlic")
    lines = ["// GENERATED by re/tools/gen_iart.py from shim/src/re/c3fv.cpp (loadInterfaceArt's full arm, after the full == 0",
             "// return): its stores, table fills, loadFlic calls and golfer-head scans in the original's order; the image loads",
             "// and cuts between them are the renderer's. Rows: {kind, a, b, c, frames, delay, flag}: IaSet {dst, value},",
             "// IaFill {dst, dwords, value}, IaFlic {id, path, -, frames, delay, flag}, IaHeads {counter, up | low << 8, base},",
             "// IaCut {sprite, w, h, -, -, kind}: a cut into a palette SpriteRef (its size and kind fields).",
             "// Included by menu.cpp. %d steps, %d loadFlic, %d cuts." % (len(steps), nf, sum(1 for x in steps if x[0] == "IaCut")), "",
             "const IaStep kIaFull[] = {"]
    for k, a, b, c, fr, de, fl, ci in steps:
        lines.append("    {%s, %s, %s, %s, %d, %d, %d},%s" % (k, a if isinstance(a, str) else hex(a), hex(b) if isinstance(b, int) else b,
                                                         c if isinstance(c, str) else hex(c), fr, de, fl,
                                                         ("  // " + ci) if ci else ""))
    lines += ["};", "const int kIaFull_n = static_cast<int>(sizeof(kIaFull) / sizeof(kIaFull[0]));", ""]
    text = "\n".join(lines)
    if "--check" in sys.argv:
        if OUT.read_text(encoding="utf-8") != text:
            sys.exit("menu_iart.inc differs from the generator's output")
        print("menu_iart.inc up to date")
        return
    OUT.write_text(text, encoding="utf-8")
    print("wrote %s: %d steps, %d loadFlic" % (OUT, len(steps), nf))


if __name__ == "__main__":
    main()
