"""S10.9 evidence collector: for every candidate of 9_cand_<key>.tsv, facts from OUR binary only.
~/bin/py -3.12 log/roadmap/S10/9_tools/collect.py <sound|jgld|Terrain>  -> log/roadmap/S10/9_facts_<key>.json
Facts: body (capstone, our extent, theirs when the master has no function), imports called, strings, callers /
callees with our current names, table slots holding the body or a thunk of it (pointer_callers.references) with the
installers' names and SMSG's vtable public at the table start (theirs: shown, never evidence)."""
import json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import capstone, xref, pointer_callers as pc  # noqa: E402

key = sys.argv[1]
mod = {"sound": "sound.dll", "jgld": "jgld.dll", "Terrain": "Terrain.dll"}[key]
m = pc.mod(mod)
pe, base = xref.load_pe(mod)
pe.parse_data_directories()
iat = {}
for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
    for i in e.imports:
        iat[i.address] = f"{e.dll.decode()}!{i.name.decode() if i.name else '#%d' % i.ordinal}"
idx = xref.load(mod)
nm = xref.names(mod)
vt = {}
for l in (ROOT / f"log/roadmap/S10/pdb/{key}.publics.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    if len(c) >= 2 and c[0].startswith("0x") and c[1].startswith("??_7"):
        vt[int(c[0], 16) + base] = c[1]


def lab(va):
    return xref.label(va, idx, nm)


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True
cands = [l.split("\t") for l in (ROOT / f"log/roadmap/S10/9_cand_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]]
out = {}
for c in cands:
    va, ours, theirs_sz = int(c[0], 16), int(c[1]), int(c[2])
    size = ours or theirs_sz
    ins = list(md.disasm(m.img[va - base:va - base + size], va))
    body = [f"{i.address:08x} {i.mnemonic} {i.op_str}".rstrip() for i in ins]
    imps = sorted({iat[op.mem.disp & 0xFFFFFFFF] for i in ins for op in i.operands
                   if op.type == capstone.x86.X86_OP_MEM and not op.mem.base and not op.mem.index
                   and (op.mem.disp & 0xFFFFFFFF) in iat})
    f = idx.get(va, {})
    refs = []
    for ref, kind, detail, inst in pc.references(m, va):
        mt = re.search(r"table at (0x[0-9a-f]+)", detail)
        refs.append({"ref": f"0x{ref:08x}", "kind": kind, "detail": detail,
                     "installers": [f"0x{x:08x} {lab(x)}" for x in inst],
                     "smsg_vtable": vt.get(int(mt.group(1), 16), "") if mt else ""})
    out[c[0]] = {"theirs": c[4], "obj": c[5], "master": c[3], "size": size, "theirs_size": theirs_sz,
                 "level": c[6], "body": body, "imports": imps,
                 "strings": [f"0x{s:08x}" for s in f.get("strings", [])],
                 "strtext": [xref.read_cstr(pe, base, s) for s in f.get("strings", [])],
                 "data": [f"0x{s:08x}" for s in f.get("data", [])],
                 "callers": [f"0x{x:08x} {lab(x)}" for x in f.get("callers", [])],
                 "callees": [f"0x{x:08x} {lab(x)}" for x in f.get("calls", [])], "refs": refs}
(ROOT / f"log/roadmap/S10/9_facts_{key}.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
print(key, len(out), "facts written")
