"""Assemble port/src/sim/audio_chan.cpp from the converter's output (log/roadmap/S4/6_conv/audio_chan.cpp), the hand-written
blocks (log/roadmap/S4/6_parts/hand.cpp, same `// ---- <name>` block shape, replacing or adding blocks) and the head
(log/roadmap/S4/6_parts/head.cpp). Adapters get their negative control (mode 1), the FuncDef table is built from the
`// FuncDef:` comments.

    ~/bin/py -3.12 log/roadmap/S4/6_assemble.py [--out port/src/sim/audio_chan.cpp]
"""
import argparse
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[3]
HERE = pathlib.Path(__file__).resolve().parent
CONV = HERE / "6_conv" / "audio_chan.cpp"
HANDS = sorted((HERE / "6_parts").glob("hand*.cpp"))
HEAD = HERE / "6_parts" / "head.cpp"

MARK = re.compile(r"^// ---- (\S+)(?: \(rva [^)]*\))?.*$", re.M)

# void functions: the dword (offset from this) mode 1 changes after the body. Default 0.
VOID_OFF = {
    "Voice_setPlayModeBits": 0x214,
    "VoiceBase_clearList": 8,
    "Channel_setVolume": 0x58,
    "Channel_setPanGains": 0x224,
    "Channel_setPitchRate": 0x220,
    "Voice_setLoopFlag": 0x2d4,
    "Voice_setPanGains": 0x48,
    "Channel_resetPanState": 0x58,
    "Channel_closeAndFree": 0x58,
    "Voice_noteOff": 0x9c,
    "Voice_setLevel": 0x50,
    "Channel_broadcastVoiceBit1": 0x58,
    "Channel_clearBackref": 0x13a4,
    "ChanRecord_freeBuffers": 0x1c,
}

# text patches of converted (not hand-written) blocks: (block, old, new)
PATCHES = [
    ("Channel_setRateGlide", "const unsigned n = duration / period;", "const unsigned n = udiv32(duration, period);"),
    ("SubChannel_dtor", "void c3dj_deleteSound(Ctx &ctx, u32 p) { static_cast<OpDelete_t>(snd_va(ctx, 0x1a87))(p); }",
     "void c3dj_deleteSound(Ctx &ctx, u32 p) { snd_call(ctx, 0x00001a87, SndAbi::Cdecl, {p}); }"),
    ("Channel_setFormat", "snd_obj(ctx, &SoundState::g_defaultWaveFormat_ea0).va()", "snd_arr(ctx, &SoundState::g_defaultWaveFormat_ea0).va()"),
    # rewindChannel: `st` is a local in the converter's output and shadows the store helper
    ("rewindChannel", "long pos = static_cast<int32_t>(", "uint32_t pos = static_cast<uint32_t>("),
    ("rewindChannel", "u32 st = ld<u32>(ctx, self + 8);", "u32 sv = ld<u32>(ctx, self + 8);"),
    ("rewindChannel", "st<uint32_t>(ctx, st + 0x20, ld<uint32_t>(ctx, st + 0x18));", "st<uint32_t>(ctx, sv + 0x20, ld<uint32_t>(ctx, sv + 0x18));"),
    ("rewindChannel", "        st = ld<u32>(ctx, self + 8);  // 0x33a55\n        st<uint32_t>(ctx, st + 0x1c, ld<uint32_t>(ctx, st + 0x20));",
     "        sv = ld<u32>(ctx, self + 8);  // 0x33a55\n        st<uint32_t>(ctx, sv + 0x1c, ld<uint32_t>(ctx, sv + 0x20));"),
    ("rewindChannel", "        st = ld<u32>(ctx, self + 8);  // 0x33a5e", "        sv = ld<u32>(ctx, self + 8);  // 0x33a5e"),
    ("rewindChannel", "ld<uint32_t>(ctx, st + 0x18));  // 0x33a69", "ld<uint32_t>(ctx, sv + 0x18));  // 0x33a69"),
]


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
    for i, ln in enumerate(lines):
        if not ln.startswith(f"void a_{name}(") or "neg_" in ln or "NEG(" in ln:
            continue
        if ln.rstrip().endswith("{"):
            continue                                   # a hand-written multi-line adapter carries its own control
        m = re.match(r"(void a_\w+\(Ctx &ctx, const uint32_t \*a, Ret &r\)) \{ (.*) \}$", ln)
        assert m, (name, ln)
        head, body = m.groups()
        r = re.match(r"r\.set_u32\((.*)\);$", body)
        if r:
            body = f"r.set_u32(neg_u32(NEG({name}), {r.group(1)}));"
        else:
            r = re.match(r"r\.set_i32\((.*)\);$", body)
            if r:
                body = f"r.set_i32(static_cast<int32_t>(neg_u32(NEG({name}), static_cast<uint32_t>({r.group(1)}))));"
            else:
                off = VOID_OFF.get(name, 0)
                body = f"{body} neg_mem(ctx, NEG({name}), a[0] + {off:#x});"
                head = head.replace("Ret &r)", "Ret &)")
        lines[i] = f"{head} {{ {body} }}"
    return "\n".join(lines)


def tidy(block):
    return re.sub(r"\{\s{20,}", "{\n    ", block)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "port" / "src" / "sim" / "audio_chan.cpp"))
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
    blocks += extra
    # trim the converter's trailer (TODO list, kConvFuncs, namespace close) off the last converted block
    fixed = []
    for n, b in blocks:
        cut = b.find("// TODO list")
        if cut >= 0:
            b = b[:cut]
        b = re.sub(r"int g_negctl_(\w+) = 0;   // TODO\(convert\)[^\n]*", r"int g_negctl_\1 = 0;", b)
        for pn, old, new in PATCHES:
            if pn == n:
                assert old in b, (n, old)
                b = b.replace(old, new)
        b = tidy(b)
        b = adapt(n, b)
        fixed.append((n, b.rstrip("\n") + "\n"))
    table = []
    for n, b in fixed:
        for ln in b.split("\n"):
            m = re.match(r"// FuncDef: (\{.*?\}),", ln)
            if m:
                table.append("    " + m.group(1) + ",")
    assert len(table) == len(fixed), (len(table), len(fixed))
    head = HEAD.read_text()
    out = [head] + [b for b in early] + ["\n".join(b for _, b in fixed)]
    out.append("\nconst FuncDef kFuncs[] = {\n" + "\n".join(table) + "\n};\n")
    out.append("}  // namespace\n\nextern const ModuleDef kModule_audio_chan = {\"audio_chan\", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};\n\n}  // namespace portsim\n")
    text = "\n".join(out)
    text = text.replace("// ---- __early shared helpers", "// ---- shared helpers")
    text = re.sub(r"^// FuncDef: [^\n]*\n", "", text, flags=re.M)
    pathlib.Path(a.out).write_text(text)
    print(f"{len(fixed)} functions, {len(text.splitlines())} lines -> {a.out}")


if __name__ == "__main__":
    main()
