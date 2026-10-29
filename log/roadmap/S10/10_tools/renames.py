"""S10.9: rename proposals for EXISTING names rows whose name is a placeholder (a hex address in the class or name part),
where SMSG's PDB names the address a constructor / destructor / deleting destructor of class C and our body stores the
vtable SMSG's public names for C (`mov dword ptr [reg], <vtable>`). Class part mapped through classmap.py.
~/bin/py -3.12 log/roadmap/S10/9_tools/renames.py <key>  -> log/roadmap/S10/9_renames_<key>.tsv"""
import glob, pathlib, re, sys

import capstone

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import classmap, vtslots  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[4]
key = sys.argv[1]
mod = vtslots.MODS[key]
m = vtslots.pc.mod(mod)
procs = vtslots.procs(key)
pubs = {va: pub for va, (pub, _) in vtslots.tables(key).items()}
cmap, _ = classmap.classmap(key)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
PH = re.compile(r"[0-9a-f]{5,}")
SPECIAL = {"`scalar deleting destructor'": "scalarDeletingDtor", "`vector deleting destructor'": "vectorDeletingDtor"}
out = []
for f in sorted(glob.glob(str(ROOT / "re/names/*.tsv"))):
    if f.endswith("_globals.tsv") or pathlib.Path(f).name.startswith("smsg_"):
        continue
    for l in open(f, encoding="utf-8").read().splitlines()[1:]:
        c = l.split("\t")
        if len(c) != 6 or c[0] != mod or not PH.search(c[2]) or c[2].startswith(("dynInit", "atexit")):
            continue
        va = int(c[1], 16) if int(c[1], 16) >= 0x10000000 else int(c[1], 16) + 0x10000000
        nm = procs.get(va, ("",))[0]
        cls, _, meth = nm.rpartition("::")
        base = cls.split("<")[0].split("::")[-1]
        kind = "ctor" if meth == base else "dtor" if meth == "~" + base else SPECIAL.get(meth)
        if not kind:
            continue
        size = next((hi - lo for lo, hi, e in m.ranges if e == va), 0)
        stores = [(i.address, int(i.op_str.split(", ")[1], 16)) for i in md.disasm(m.img[va - m.base:va - m.base + size], va)
                  if i.mnemonic == "mov" and re.match(r"dword ptr \[e[a-z]{2}\], 0x1", i.op_str)]
        hit = [(a, t) for a, t in stores if pubs.get(t, "").startswith(f"??_7{base}@@")]
        if not hit:
            continue
        new = f"{cmap.get(cls, cls)}::{kind}"
        out.append(f"{pathlib.Path(f).name}\t{c[1]}\t{c[2]}\t{new}\tstores vtable 0x{hit[0][1]:08x} at 0x{hit[0][0]:08x}; "
                   f"SMSG's public there is {pubs[hit[0][1]]} and its PDB names the function {nm}")
(ROOT / f"log/roadmap/S10/9_renames_{key}.tsv").write_text("file\taddr\told\tnew\tevidence\n" + "\n".join(out) + "\n", encoding="utf-8")
print(key, len(out), "rename proposals")
