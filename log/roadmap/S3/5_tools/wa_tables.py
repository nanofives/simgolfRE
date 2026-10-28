"""loadWorldAssets' operation tables in shim/src/re/c3fv.cpp (the C3 source that cites every call site): list each kWa* table
with its op counts, or print the palette loads (kLoad with a palette argument) as the generated block of gfx_scene.cpp.

    ~/bin/py -3.12 log/roadmap/S3/5_tools/wa_tables.py survey
    ~/bin/py -3.12 log/roadmap/S3/5_tools/wa_tables.py gen            # the C++ block (GSO offsets, no addresses)
    ~/bin/py -3.12 log/roadmap/S3/5_tools/wa_tables.py check          # the block in port/src/sim/gfx_scene.cpp is current
"""
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
SRC = ROOT / "shim" / "src" / "re" / "c3fv.cpp"
ADDR = ROOT / "re" / "model" / "addresses.tsv"
TARGET = ROOT / "port" / "src" / "sim" / "gfx_scene.cpp"
BEGIN = "// BEGIN GENERATED loadWorldAssets palette loads (log/roadmap/S3/5_tools/wa_tables.py gen)"
END = "// END GENERATED loadWorldAssets palette loads"

TABLE_RE = re.compile(r"^const Op (kWa\w+)\[\] = \{\s*$")
OP_RE = re.compile(r"^\s*\{(kFlic|kLoad|kBlit|kSet),\s*([^}]*)\},\s*//\s*(0x[0-9a-fA-F]+)")


def tables():
    out, cur = {}, None
    for ln in SRC.read_text(encoding="utf-8").splitlines():
        m = TABLE_RE.match(ln)
        if m:
            cur = m.group(1)
            out[cur] = []
            continue
        if cur is None:
            continue
        if ln.startswith("};"):
            cur = None
            continue
        m = OP_RE.match(ln)
        if m:
            vals = [int(v, 16) for v in m.group(2).replace(" ", "").split(",") if v]
            out[cur].append((m.group(1), vals, m.group(3)))
    return out


def symbols():
    rows = []
    for r in list(csv.reader(open(ADDR, encoding="utf-8"), delimiter="\t"))[1:]:
        try:
            rows.append((int(r[2], 16), int(r[3], 16), r[0]))
        except (ValueError, IndexError):
            pass
    rows.sort()
    return rows


def gso(rows, va):
    for a, n, s in rows:
        if a <= va < a + n:
            return f"GSO({s}, 0x{va - a:x})"
    raise SystemExit(f"no model object holds 0x{va:08x}")


def gen():
    rows = symbols()
    lines = [BEGIN]
    lines.append("// {path, palette object}: each a loadImage(working image, path, palette object, 0, 0x100, 0) of the table (the C3")
    lines.append("// source's call site in the comment, as loadWorldAssets+<offset>); GSO = an offset in the GameState")
    for name, ops in tables().items():
        loads = [(v, site) for k, v, site in ops if k == "kLoad" and len(v) >= 2 and v[1] != 0]
        if not loads:
            continue
        lines.append(f"const PalLoad {name}[] = {{")
        for v, site in loads:
            if v[2] != 0 or v[3] != 0x100 or v[4] != 0:
                raise SystemExit(f"{name}: a load with ofs / count / flags other than 0, 0x100, 0 at {site}")
            off = int(site, 16) - 0x0043DBE0
            lines.append(f"    {{{gso(rows, v[0])}, {gso(rows, v[1])}}},  // loadWorldAssets+0x{off:x}")
        lines.append("};")
    # the palette loads LoadWorldAssets_re writes out (Pal(&img, path, palette)): the water palette per course type, then the
    # tails of the tree (D1), flower (D3) and tall-tree (D4, through `last`) palette groups, in the body's order
    body = SRC.read_text(encoding="utf-8")
    a = body.index("void __cdecl LoadWorldAssets_re()")
    body = body[a:body.index("\n}\n", a)]
    pal = re.findall(r"Pal\(&img, (0x[0-9A-Fa-f]+), (0x[0-9A-Fa-f]+)\)", body)
    last = re.findall(r"last = (0x[0-9A-Fa-f]+);", body)
    names = ["kPalWaterDesert", "kPalWaterTrop", "kPalWaterLinks", "kPalWaterPark", "kPalTreeDesert", "kPalTreeTrop",
             "kPalTreeOther", "kPalFlowerDesert", "kPalFlowerTrop", "kPalFlowerOther"]
    tall_m = re.findall(r"Pal\(&img, last, (0x[0-9A-Fa-f]+)\)", body)
    if len(pal) != len(names) or len(last) != 4 or len(tall_m) != 1:
        raise SystemExit(f"LoadWorldAssets_re: {len(pal)} Pal calls, {len(last)} `last` paths (expected 10, 4)")
    for n, (p, d) in zip(names, pal):
        lines.append(f"const PalLoad {n} = {{{gso(rows, int(p, 16))}, {gso(rows, int(d, 16))}}};")
    tall = int(tall_m[0], 16)
    for n, p in zip(["kPalTallLinks2", "kPalTallDesert", "kPalTallTrop", "kPalTallOther"], last):
        lines.append(f"const PalLoad {n} = {{{gso(rows, int(p, 16))}, {gso(rows, tall)}}};")
    lines.append(END)
    return "\n".join(lines) + "\n"


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "survey"
    if cmd == "survey":
        for name, ops in tables().items():
            kinds = {}
            for k, v, _ in ops:
                key = k + ("+pal" if k == "kLoad" and len(v) > 1 and v[1] else "")
                kinds[key] = kinds.get(key, 0) + 1
            print(name, kinds)
    elif cmd == "gen":
        sys.stdout.write(gen())
    elif cmd == "check":
        t = TARGET.read_text(encoding="utf-8").replace("\r\n", "\n")
        a, b = t.index(BEGIN), t.index(END) + len(END) + 1
        if t[a:b] != gen():
            raise SystemExit("gfx_scene.cpp's generated block is stale")
        print("gfx_scene.cpp generated block current")


if __name__ == "__main__":
    main()
