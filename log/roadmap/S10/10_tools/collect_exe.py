"""S10.10 evidence collector for the exe rows of 8_methods.tsv that S10.8 did not rename.
~/bin/py -3.12 log/roadmap/S10/10_tools/collect_exe.py  -> log/roadmap/S10/10_facts_exe.json
Facts from OUR binary only: body (capstone, our extent), callers / callees under our names, table slots holding the body
(pointer_callers.references) with installers. golf.pdb: every procedure / public at the address (ICF count, U-1850)."""
import json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import capstone, xref, pointer_callers as pc  # noqa: E402

MOD, BASE = "golf_clean.exe", 0x400000
m = pc.mod(MOD)
idx = xref.load(MOD)
nm = xref.names(MOD)
RENAMED = {0x487490, 0x4874a0, 0x4874b0, 0x4874d0, 0x4874f0, 0x487610, 0x487680, 0x4876a0, 0x487810, 0x487890}
pdb = {}
for f, col in (("golf.procs.tsv", 2), ("golf.publics.tsv", 1)):
    for l in (ROOT / "log/roadmap/S10/pdb" / f).read_text(encoding="utf-8").splitlines():
        c = l.split("\t")
        if len(c) > col and c[0].startswith("0x"):
            pdb.setdefault(int(c[0], 16) + BASE, set()).add(c[col])
size = {}
for l in (ROOT / "re/functions_ghidra.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    c = l.split("\t")
    try:
        size[int(c[0], 16)] = int(c[2])
    except (ValueError, IndexError):
        pass


def lab(va):
    return xref.label(va, idx, nm)


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
out = {}
for l in (ROOT / "log/roadmap/S10/8_methods.tsv").read_text(encoding="utf-8").splitlines():
    c = l.split("\t")
    va = int(c[0], 16)
    if va in RENAMED:
        continue
    sz = size.get(va) or int(c[3])
    ins = list(md.disasm(m.img[va - BASE:va - BASE + sz], va))
    f = idx.get(va, {})
    refs = []
    for ref, kind, detail, inst in pc.references(m, va):
        refs.append({"ref": f"0x{ref:08x}", "kind": kind, "detail": detail,
                     "installers": [f"0x{x:08x} {lab(x)}" for x in inst]})
    out[c[0]] = {"ours": c[1], "theirs": c[2], "size": sz, "level": c[4],
                 "pdb_names": sorted(pdb.get(va, set())),
                 "body": [f"{i.address:08x} {i.mnemonic} {i.op_str}".rstrip() for i in ins],
                 "callers": [f"0x{x:08x} {lab(x)}" for x in f.get("callers", [])],
                 "callees": [f"0x{x:08x} {lab(x)}" for x in f.get("calls", [])], "refs": refs}
(ROOT / "log/roadmap/S10/10_facts_exe.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
print(len(out), "facts written")
