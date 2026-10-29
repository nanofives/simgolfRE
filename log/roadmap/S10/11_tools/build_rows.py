"""S10.9: names rows from 9_verdict_<key>.tsv (mechanical ACCEPT_*) + 9_manual_<key>*.tsv (hand verdicts).
~/bin/py -3.12 log/roadmap/S10/9_tools/build_rows.py sound <names id> <subsystem>  -> re/names/<id>.tsv + 9_ledger_<key>.tsv"""
import glob, json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
key, nid, sub = sys.argv[1], sys.argv[2], sys.argv[3]
mod = {"sound": "sound.dll", "jgld": "jgld.dll", "Terrain": "Terrain.dll"}[key]
facts = json.loads((ROOT / f"log/roadmap/S10/9_facts_{key}.json").read_text(encoding="utf-8"))
verd = [l.split("\t") for l in (ROOT / f"log/roadmap/S10/9_verdict_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]]
manual = {}
for f in sorted(glob.glob(str(ROOT / f"log/roadmap/S10/9_manual_{key}*.tsv"))):
    for l in pathlib.Path(f).read_text(encoding="utf-8").splitlines()[1:]:
        c = l.split("\t")
        manual[c[0]] = c
taken = set()
for f in glob.glob(str(ROOT / "re/names/*.tsv")):
    if f.endswith("_globals.tsv") or pathlib.Path(f).stem == nid:
        continue
    for l in open(f, encoding="utf-8").read().splitlines()[1:]:
        c = l.split("\t")
        if len(c) == 6 and c[0] == mod:
            taken.add(c[2])
for l in (ROOT / f"re/functions_ghidra_{mod}.tsv").read_text().splitlines()[1:]:
    taken.add(l.split("\t")[1])


def field(s):
    return re.sub(r"(byte|word|dword|qword) ptr \[ecx \+ (0x[0-9a-f]+)\]", r"\1 this+\2", s).replace("dword ptr [ecx]", "dword this+0")


def pin(v):
    for r in v["refs"]:
        if r["installers"]:
            return f"{r['detail'].replace(' (+', ' (+')} (installer {r['installers'][0]})"
    named = [c for c in v["callers"] if "FUN_" not in c]
    return f"caller {named[0]}" if named else "no installer"


rows, ledger = [], []
for va, verdict, smsg, name, kd, det in verd:
    v = facts[va]
    src = f"name from SMSG-decomp's PDB (log/roadmap/S10/pdb/{key}.procs.tsv row 0x{int(va, 16) - 0x10000000:x}: {smsg})"
    if va in manual:
        m = manual[va]
        ledger.append(f"{va}\t{'ACCEPT_HAND' if m[1] == 'A' else 'REJECT_HAND'}\t{smsg}\t{m[2]}\t{m[3] if m[1] == 'R' else ''}")
        if m[1] == "A":
            own = m[2].startswith(("dynInit_", "atexitDtor_", "construct"))
            note = f"SMSG-decomp's PDB calls it {smsg} (compiler name); the name follows our dynInit_ / atexitDtor_ / construct convention" if own else src
            rows.append([va, m[2], f"{m[3]}; {note}", m[4]])
        continue
    ledger.append(f"{va}\t{verdict}\t{smsg}\t{name}\t{det if not verdict.startswith('ACCEPT') else ''}")
    if not verdict.startswith("ACCEPT"):
        continue
    start = v["body"][0].split()[0]
    if verdict == "ACCEPT_GET":
        purpose = f"Read-only accessor: returns a value computed from {field(det.split('reads ')[1].split(' (from')[0])}; stores nothing, calls nothing."
    elif verdict == "ACCEPT_SET":
        purpose = f"Writes {field(det.split('stores ')[1].split(' from')[0]) if 'from its' in det else field(det)} of this; calls nothing."
    elif verdict == "ACCEPT_FWD":
        purpose = "Forwards the call with its arguments to " + field(det.split("; slot")[0].replace("calls ", "")) + "."
    else:
        purpose = {"ctor": "Constructor", "dtor": "Destructor", "scalarDeletingDtor": "Scalar deleting destructor",
                   "vectorDeletingDtor": "Vector deleting destructor"}[name.split("::")[-1]] + " of the class whose vtable it stores or frees."
    rows.append([va, name, f"{pin(v)}; body at 0x{start} ({v['size']} bytes): {det}; {src}", purpose])
sys.path.insert(0, str(pathlib.Path(__file__).parent))
import classmap  # noqa: E402
cmap, _ = classmap.classmap(key)
final, seen = [], set()
for va, name, ev, purpose in rows:
    cls, sep, meth = name.rpartition("::")
    mapped = f"{cmap[cls]}::{meth}" if sep and cls in cmap else name
    if mapped != name:
        ev = f"{ev}; SMSG's class {cls} is our {cmap[cls]} (log/roadmap/S10/9_tools/classmap.py)"
    if mapped in taken or mapped in seen:
        mapped = f"{mapped}_{va[2:]}"
    seen.add(mapped)
    final.append([va, name, mapped, ev, purpose])
# a row's text may cite another new row by its SMSG name: rewrite those mentions (never a name our files already use)
ren = sorted(((n, m) for _, n, m, _, _ in final if n != m and n not in taken), key=lambda x: -len(x[0]))
out = []
for va, name, mapped, ev, purpose in final:
    for a, b in ren:
        ev, purpose = (re.sub(re.escape(a) + r"(?![\w:])", b, s) for s in (ev, purpose))
    out.append("\t".join([mod, va, mapped, sub, ev.replace("\t", " "), purpose.replace("\t", " ")]))
(ROOT / f"re/names/{nid}.tsv").write_text("module\taddr\tname\tsubsystem\tevidence\tpurpose\n" + "\n".join(out) + "\n", encoding="utf-8")
(ROOT / f"log/roadmap/S10/9_ledger_{key}.tsv").write_text("va\tverdict\tsmsg\tname\treason\n" + "\n".join(ledger) + "\n", encoding="utf-8")
missing = [va for va, verdict, *_ in verd if verdict == "MANUAL" and va not in manual]
print(f"{nid}: {len(out)} rows; MANUAL without a hand verdict: {len(missing)} {missing[:8]}")
