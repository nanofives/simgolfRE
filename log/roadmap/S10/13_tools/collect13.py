"""S10.13 fact collector for the 145 deferred bodies (log/roadmap/S10/9_deferred_not_functions.tsv).
~/bin/py -3.12 log/roadmap/S10/13_tools/collect13.py  -> log/roadmap/S10/13_facts.json
The bodies are not functions in our lists, so xref.py has no row for them: the body is decoded from the PDB extent
(6_boundaries_sound.tsv's size, which is code-only for three), callers come from a raw scan of the image for
`call rel32` / `jmp rel32` / `jcc rel32` onto the body or one of its incremental-link thunks, installers from
pointer_callers.references. Facts from OUR binary only; the SMSG name is carried as the hypothesis."""
import json, pathlib, struct, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
S10 = ROOT / "log/roadmap/S10"
sys.path.insert(0, str(ROOT / "re" / "tools"))
import capstone, xref, pointer_callers as pc  # noqa: E402

KEY = {"sound.dll": "sound", "jgld.dll": "jgld", "Terrain.dll": "Terrain"}
rows = [l.split("\t") for l in (S10 / "9_deferred_not_functions.tsv").read_text(encoding="utf-8").splitlines()[1:] if l]
bsize = {}
for l in (S10 / "6_boundaries_sound.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if len(c) >= 3 and c[1].startswith("0x"):
        bsize[(c[0], int(c[1], 16))] = int(c[2])
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True
ext = {(r[0], int(r[1], 16)): bsize.get((r[0], int(r[1], 16)), int(r[3])) for r in rows}


def raw_branches(m, wanted):
    """{target: [(site, mnemonic)]} for every rel32 call / jmp / jcc in .text onto an address of `wanted` that decodes
    as that instruction at that site (one pass over the module)."""
    out = {}
    for lo, hi in m.text:
        img = m.img[lo - m.base:hi - m.base]
        for i in range(len(img) - 6):
            b = img[i]
            if b == 0xE8 or b == 0xE9:
                n, at = 5, i
            elif b == 0x0F and 0x80 <= img[i + 1] <= 0x8F:
                n, at = 6, i + 1
            else:
                continue
            t = lo + i + n + struct.unpack_from("<i", img, at + 1)[0]
            if t in wanted:
                ins = next(md.disasm(img[i:i + n], lo + i), None)
                if ins is not None and ins.size == n:
                    out.setdefault(t, []).append((lo + i, ins.mnemonic))
    return out


def owner(m, site, defer):
    for (lo, hi, nm) in defer:
        if lo <= site < hi:
            return f"deferred 0x{lo:08x} {nm}"
    f = m.function_at(site)
    return f"0x{f:08x} {m.entries.get(f, '?')}" if f else "outside every function"


out = {}
for mod in KEY:
    m = pc.mod(mod)
    pe, base = xref.load_pe(mod)
    pe.parse_data_directories()
    iat = {i.address: f"{e.dll.decode()}!{i.name.decode() if i.name else '#%d' % i.ordinal}"
           for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []) for i in e.imports}
    nm = xref.names(mod)
    mine = [r for r in rows if r[0] == mod]
    defer = [(int(r[1], 16), int(r[1], 16) + ext[(mod, int(r[1], 16))], r[2]) for r in mine]
    dname = {lo: n for lo, _, n in defer}
    wanted = set()
    for lo, _, _ in defer:
        wanted |= {lo, *m.thunks.get(lo, [])}
    br = raw_branches(m, wanted)
    thunk_of = {t: b for b, ts in m.thunks.items() for t in ts}

    def lab(t):
        t = thunk_of.get(t, t)
        if t in dname:
            return f"0x{t:08x} deferred {dname[t]}"
        n = nm.get(t)
        if n and n[0] == "names":
            return f"0x{t:08x} {n[1]}"
        return f"0x{t:08x} {m.entries.get(t, '?')}" + (f" [match {n[1]}]" if n else "")
    for lo, hi, smsg in defer:
        ins = list(md.disasm(m.img[lo - base:hi - base], lo))
        body = [f"{i.address:08x} {i.mnemonic} {i.op_str}".rstrip() for i in ins]
        decoded = sum(i.size for i in ins)
        calls, imps = [], []
        for i in ins:
            for op in i.operands:
                if op.type == capstone.x86.X86_OP_MEM and not op.mem.base and not op.mem.index \
                        and (op.mem.disp & 0xFFFFFFFF) in iat:
                    imps.append(iat[op.mem.disp & 0xFFFFFFFF])
            if i.mnemonic in ("call", "jmp") and i.operands and i.operands[0].type == capstone.x86.X86_OP_IMM:
                t = i.operands[0].imm & 0xFFFFFFFF
                if not lo <= t < hi:
                    calls.append(f"{i.mnemonic} " + lab(t))
        callers = []
        for t in [lo, *m.thunks.get(lo, [])]:
            for site, mn in br.get(t, []):
                callers.append(f"0x{site:08x} {mn} 0x{t:08x} from {owner(m, site, defer)}")
        refs = [{"ref": f"0x{r:08x}", "kind": k, "detail": d, "installers": [f"0x{x:08x} {m.entries.get(x, '?')}" for x in inst]}
                for r, k, d, inst in pc.references(m, lo)]
        out[f"{mod} 0x{lo:08x}"] = {"module": mod, "va": f"0x{lo:08x}", "smsg": smsg, "size": hi - lo,
                                    "decoded": decoded, "body": body, "imports": sorted(set(imps)), "calls": calls,
                                    "thunks": [f"0x{t:08x}" for t in m.thunks.get(lo, [])],
                                    "callers": callers, "refs": refs}
(S10 / "13_facts.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
print(len(out), "facts;", sum(1 for v in out.values() if v["callers"] or v["refs"]), "with a caller or reference")
