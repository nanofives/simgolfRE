"""The per-pixel blend of Sprite::drawAlpha's RGB555 bodies, read from re/match/jgld_blit16.cpp (100% matched sources).

For each body the first unscaled __asm loop's pixel block (from the payload load to the word store) is extracted and run
symbolically: the result is a straight-line list of 32-bit register operations over P (the palette word) and D (the
destination word). `gen` prints the C++ table gfx_scene.cpp embeds; `show` prints the blocks.

    ~/bin/py -3.12 log/roadmap/S3/5_tools/alpha_ops.py show|gen|check
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
SRC = ROOT / "re" / "match" / "jgld_blit16.cpp"
TARGET = ROOT / "port" / "src" / "sim" / "gfx_scene.cpp"
BEGIN = "// BEGIN GENERATED drawAlpha blend programs (log/roadmap/S3/5_tools/alpha_ops.py gen)"
END = "// END GENERATED drawAlpha blend programs"

# drawAlpha's RGB555 arm (format word 1: is565 false): the alpha window -> body (re/match/jgld_blitY17.cpp)
BODIES = ["10041f80", "10043430", "10044940", "10045e50", "100473b0", "10048910", "10049ea0", "1004b430", "1004ca10",
          "1004dff0", "1004f5d0", "10050bb0", "100521d0", "100537f0", "10054e20"]
# the same windows' members when drawAlpha's flag byte stays 1 (destination format word +4 != 1): drawAlpha's arms
# 0x10018b52 .. 0x100190e7 call the first of each pair when the flag is set (0x1001907d: draw16_1002ed60 for the quarter band)
BODIES_F = ["1002d890", "1002ed60", "10030290", "100317c0", "10032d60", "10034300", "100358b0", "10036e60", "10038460",
            "10039a60", "1003b060", "1003c660", "1003dca0", "1003f2e0", "10040930"]
REGS = ["eax", "ebx", "ecx", "edx", "esi"]
OPS = {"mov": 0, "shl": 1, "shr": 2, "and": 3, "add": 4, "sub": 5}


def body_text(body):
    t = SRC.read_text(encoding="utf-8")
    a = t.index(f"// MATCH: jgld.dll 0x{body} ")
    z = t.find("// MATCH:", a + 10)
    return t[a:z if z > 0 else len(t)]


def blocks(body):
    """every pixel block of the body (all unscaled and scaled loops), from the payload load to the word store"""
    lines = [ln.strip() for ln in body_text(body).splitlines()]
    out = []
    for i, ln in enumerate(lines):
        if ln in ("mov al, byte ptr [esi]", "mov dl, byte ptr [esi]") and lines[i - 1] == "xor edx, edx":
            e = next(j for j in range(i, len(lines)) if lines[j].startswith("mov word ptr [edi]"))
            out.append(tuple(lines[i:e + 1]))
    return out


def block(body):
    bs = blocks(body)
    if len(set(bs)) != 1:
        raise SystemExit(f"{body}: {len(set(bs))} distinct pixel blocks in {len(bs)} loops")
    return list(bs[0])


LOADS = {"mov dx, word ptr [edi]": ("mov", "edx", "D"), "mov ax, word ptr [edi]": ("mov", "eax", "D"),
         "mov ax, word ptr [ecx + eax*2]": ("mov", "eax", "P"), "mov dx, word ptr [ecx + edx*2]": ("mov", "edx", "P")}


def program(body):
    """(op, dst, src) triples; src a register name, an int, or P / D (the palette and destination words, zero-extended)"""
    prog = []
    for ln in block(body):
        if ln in ("mov al, byte ptr [esi]", "mov dl, byte ptr [esi]") or ln.startswith(("push", "pop")):
            continue
        if ln in LOADS:
            prog.append(LOADS[ln])
            continue
        if ln == "mov word ptr [edi], ax":
            continue
        m = re.match(r"(mov|shl|shr|and|add|sub) (e[abcd]x|esi), (e[abcd]x|esi|0x[0-9a-f]+|[0-9]+)$", ln)
        if not m:
            raise SystemExit(f"{body}: cannot read `{ln}`")
        op, dst, src = m.groups()
        prog.append((op, dst, int(src, 16) if src.startswith("0x") else int(src) if src.isdigit() else src))
    return prog


def gen():
    lines = [BEGIN, "// One step per instruction of the pixel block: {op, dst, src kind, src}; registers 0 eax 1 ebx 2 ecx 3 edx 4 esi;",
             "// src kind 0 a register, 1 an immediate, 2 the palette word P, 3 the destination word D. The store keeps eax's low",
             "// 16 bits. Read from re/match/jgld_blit16.cpp: all eight loops of each body hold this block (the tool checks it)."]
    lines.append("struct AStep {\n  uint8_t op, dst, kind;\n  uint32_t src;\n};")
    lines.append("struct AProgram {\n  const char *body;\n  const AStep *steps;\n  size_t n;\n};")
    for b in BODIES + BODIES_F:
        steps = []
        for op, dst, src in program(b):
            if op == "store":
                continue
            d = REGS.index(dst)
            if src == "P":
                steps.append(f"{{0, {d}, 2, 0}}")
            elif src == "D":
                steps.append(f"{{0, {d}, 3, 0}}")
            elif isinstance(src, int):
                steps.append(f"{{{OPS[op]}, {d}, 1, 0x{src:x}}}")
            else:
                steps.append(f"{{{OPS[op]}, {d}, 0, {REGS.index(src)}}}")
        lines.append(f"const AStep kA_{b}[] = {{" + ", ".join(steps) + "};")
    lines.append("// [0, 15): the flag-cleared members (destination format word +4 == 1), [15, 30): the flag-set members")
    lines.append("const AProgram kAlphaPrograms[] = {")
    for b in BODIES + BODIES_F:
        lines.append(f"    {{\"draw16_{b}\", kA_{b}, sizeof(kA_{b}) / sizeof(kA_{b}[0])}},")
    lines.append("};")
    lines.append(END)
    return "\n".join(lines) + "\n"


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "show"
    if cmd == "show":
        for b in BODIES + BODIES_F:
            print(b, len(blocks(b)), "loops:", " ; ".join(f"{o} {d},{s}" for o, d, s in program(b)))
    elif cmd == "gen":
        sys.stdout.write(gen())
    elif cmd == "check":
        t = TARGET.read_text(encoding="utf-8").replace("\r\n", "\n")
        a, z = t.index(BEGIN), t.index(END) + len(END) + 1
        if t[a:z] != gen():
            raise SystemExit("gfx_scene.cpp's drawAlpha block is stale")
        print("gfx_scene.cpp drawAlpha block current")


if __name__ == "__main__":
    main()
