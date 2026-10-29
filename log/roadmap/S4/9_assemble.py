"""Assemble port/src/sim/audio_wave.cpp from the converter's output (log/roadmap/S4/9_conv/audio_wave.cpp), the hand-written
blocks (log/roadmap/S4/9_parts/hand.cpp, same `// ---- <name>` block shape, replacing or adding blocks) and the head
(log/roadmap/S4/9_parts/head.cpp). Adapters get their negative control (mode 1), the FuncDef table is built from the
`// FuncDef:` comments.

    ~/bin/py -3.12 log/roadmap/S4/9_assemble.py [--out port/src/sim/audio_wave.cpp]
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
HERE = pathlib.Path(__file__).resolve().parent
CONV = HERE / "9_conv" / "audio_wave.cpp"
HANDS = sorted((HERE / "9_parts").glob("hand*.cpp"))
HEAD = HERE / "9_parts" / "head.cpp"

MARK = re.compile(r"^// ---- (\S+)(?: \(rva [^)]*\))?.*$", re.M)

sys.path.insert(0, str(HERE))
from s9_patches import PATCHES, VOID_OFF, GLOBAL_RE  # noqa: E402


def split_blocks(text):
    ms = list(MARK.finditer(text))
    pre = text[:ms[0].start()] if ms else text
    out = []
    for i, m in enumerate(ms):
        end = ms[i + 1].start() if i + 1 < len(ms) else len(text)
        out.append((m.group(1), text[m.start():end]))
    return pre, out


def adapt(name, block):
    """negative-control mode 1 in the adapter line"""
    lines = block.split("\n")
    nm = name.replace("::", "__")                      # the C++ identifier of a Class::method block
    for i, ln in enumerate(lines):
        if not ln.startswith(f"void a_{nm}(") or "neg_" in ln or "NEG(" in ln:
            continue
        if ln.rstrip().endswith("{"):
            continue                                   # a hand-written multi-line adapter carries its own control
        m = re.match(r"(void a_\w+\(Ctx &ctx, const uint32_t \*a, Ret &r\)) \{ (.*) \}$", ln)
        assert m, (name, ln)
        head, body = m.groups()
        r = re.match(r"r\.set_u32\((.*)\);$", body)
        if r:
            body = f"r.set_u32(neg_u32(NEG({nm}), {r.group(1)}));"
        else:
            r = re.match(r"r\.set_i32\((.*)\);$", body)
            if r:
                body = f"r.set_i32(static_cast<int32_t>(neg_u32(NEG({nm}), static_cast<uint32_t>({r.group(1)}))));"
            else:
                off = VOID_OFF.get(name, 0)
                body = f"{body} neg_mem(ctx, NEG({nm}), a[0] + {off:#x});"
                head = head.replace("Ret &r)", "Ret &)")
        if "a[" not in body:
            head = head.replace("const uint32_t *a,", "const uint32_t *,")       # no argument slot read: no unused-parameter warning
        lines[i] = f"{head} {{ {body} }}"
    return "\n".join(lines)


def tidy(block):
    return re.sub(r"\{\s{20,}", "{\n    ", block)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "port" / "src" / "sim" / "audio_wave.cpp"))
    a = ap.parse_args()
    _, conv = split_blocks(CONV.read_text())
    hand = []
    for hf in HANDS:
        hand += split_blocks(hf.read_text())[1]
    hmap = dict(hand)
    cnames = [n for n, _ in conv]
    blocks = []
    early = [b for n, b in hand if n.startswith("__early")]
    for n, b in conv:
        if n.startswith("// TODO list") or b.lstrip().startswith("// TODO list"):
            continue
        blocks.append((n, hmap.get(n, b)))
    blocks = [(n, b) for n, b in blocks]
    extra = [(n, b) for n, b in hand if n not in cnames and not n.startswith("__early")]
    # a block with no converter output goes where its rva puts it (its helpers are used by blocks after it, and it uses the ones before it)
    def rva_of(b):
        m = re.search(r"\(rva (0x[0-9a-fA-F]+)\)", b.split("\n", 1)[0])
        return int(m.group(1), 16) if m else None
    for n, b in extra:
        r = rva_of(b)
        assert r is not None, f"{n}: a hand block with no converter output needs `(rva 0x...)` in its marker"
        pos = next((i for i, (_, cb) in enumerate(blocks) if (rva_of(cb) or 0) > r), len(blocks))
        blocks.insert(pos, (n, b))
    # trim the converter's trailer (TODO list, kConvFuncs, namespace close) off the last converted block
    fixed, negs = [], []
    for n, b in blocks:
        cut = b.find("// TODO list")
        if cut >= 0:
            b = b[:cut]
        b = re.sub(r"int g_negctl_(\w+) = 0;   // TODO\(convert\)[^\n]*", r"int g_negctl_\1 = 0;", b)
        for pt in PATCHES:
            if pt[0] != n or n in hmap:       # patches are for the converter's blocks; a hand block is already final
                continue
            if len(pt) == 4:
                b, cnt = re.subn(pt[1], lambda m, r=pt[2]: r, b, count=1, flags=re.S)
                assert cnt == 1, (n, pt[1])
            else:
                assert pt[1] in b, (n, pt[1])
                b = b.replace(pt[1], pt[2])
        for gp, gr in GLOBAL_RE:
            b = re.sub(gp, gr, b)
        b = tidy(b)
        b = adapt(n, b)
        negs.extend(re.findall(r"^int g_negctl_\w+ = 0;$", b, re.M))      # declared together before the first body: a body may read its own mode
        b = re.sub(r"^int g_negctl_\w+ = 0;\n", "", b, flags=re.M)
        fixed.append((n, b.rstrip("\n") + "\n"))
    table = []
    for n, b in fixed:
        for ln in b.split("\n"):
            m = re.match(r"// FuncDef: (\{.*?\}),", ln)
            if m:
                table.append("    " + m.group(1) + ",")
    assert len(table) == len(fixed), (len(table), len(fixed))
    head = HEAD.read_text()
    out = [head] + ["// ---- the negative-control modes (read by the adapters and, for the import-level controls, by the bodies)\n"
                    + "\n".join(negs) + "\n"] + [b for b in early] + ["\n".join(b for _, b in fixed)]
    out.append("\nconst FuncDef kFuncs[] = {\n" + "\n".join(table) + "\n};\n")
    out.append("}  // namespace\n\nextern const ModuleDef kModule_audio_wave = {\"audio_wave\", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};\n\n}  // namespace portsim\n")
    text = "\n".join(out)
    text = text.replace("// ---- __early shared helpers", "// ---- shared helpers")
    text = re.sub(r"^// FuncDef: [^\n]*\n", "", text, flags=re.M)
    text = re.sub(r"^// TODO\(convert\): [^\n]*\n\n?", "", text, flags=re.M)
    pathlib.Path(a.out).write_text(text)
    print(f"{len(fixed)} functions, {len(text.splitlines())} lines -> {a.out}")


if __name__ == "__main__":
    main()
