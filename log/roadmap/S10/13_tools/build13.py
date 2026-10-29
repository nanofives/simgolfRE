"""S10.13: verdicts and names rows for the 145 deferred bodies.
~/bin/py -3.12 log/roadmap/S10/13_tools/build13.py
Reads 13_facts.json (collect13.py), 13_neighbours.tsv (neighbours.py) and the hand verdicts 13_manual.tsv
(va, v, name-or-rule, pin, evidence, purpose). Every body without a hand row is a constant / empty stub (REJECT_STUB)
or has no class pin (REJECT_NO_CLASS_PIN). Writes log/roadmap/S10/13_ledger.tsv and re/names/smsg_sound_2.tsv."""
import collections, json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
S10 = ROOT / "log/roadmap/S10"
sys.path.insert(0, str(pathlib.Path(__file__).parent))
import shape  # noqa: E402

facts = json.loads((S10 / "13_facts.json").read_text(encoding="utf-8"))
nb = {c[0]: c for c in (l.split("\t") for l in (S10 / "13_neighbours.tsv").read_text(encoding="utf-8").splitlines()[1:])}
man = {c[0]: c for c in (l.split("\t") for l in (S10 / "13_manual.tsv").read_text(encoding="utf-8").splitlines()[1:]) if c[0]}
PIN = {"FREE": "no class: SMSG names a free function",
       "VT": "class pin: the vtable the body stores (cited below)",
       "THIS": "class pin: the call that passes this (cited below)"}


def reach(v):
    own = set(v["thunks"])
    other = [c for c in v["callers"] if c.split()[0] not in own]
    txt = (f"not a function in our list (extent {v['size']} bytes, log/roadmap/S10/6_boundaries_sound.tsv); "
           f"incremental-link thunk {', '.join(v['thunks']) or 'none'} jumps to it")
    stores = [r for r in v["refs"] if r["installers"]]
    if not other and not stores:
        return txt + ", and no instruction or data references the thunk or the body: dead in this build", True
    bits = [c.replace(" from deferred ", " from the dead body ") for c in other]
    bits += [f"{r['ref']} {r['kind']} by {', '.join(r['installers'])}" for r in stores]
    live = any("deferred" not in c and "outside every function" not in c for c in other) or bool(stores)
    return txt + "; referenced by " + "; ".join(bits) + ("" if live else ": reached only from dead code"), not live


def pin_text(va, pin):
    if pin == "NEIGHBOUR":
        c = nb[va]
        return (f"class pin: the nearest named functions with a class prefix, {c[3]} below and {c[4]} above, carry our "
                f"class {c[2]} (VC6 lays out the functions of one object file contiguously; "
                f"log/roadmap/S10/13_tools/neighbours.py), the class log/roadmap/S10/13_tools/classmap.py maps SMSG's to")
    return PIN.get(pin, "")


ledger, rows, cnt = ["module\tva\tsmsg\tverdict\tname\tpin\treason"], [], collections.Counter()
for k, v in facts.items():
    va, mod, smsg = v["va"], v["module"], v["smsg"]
    r, dead = reach(v)
    m = man.get(va)
    if m and m[1] == "A":
        name, pin, ev, purpose = m[2], m[3], m[4], m[5]
        rule = ev.split(":")[0] if ev.startswith("ACCEPT_") else "ACCEPT_HAND"
        ev = ev.split(": ", 1)[1] if ev.startswith("ACCEPT_") else ev
        rva = int(va, 16) - 0x10000000
        evid = (f"{rule}; {r}; {pin_text(va, pin)}; body: {ev}; name from SMSG-decomp's PDB "
                f"(log/roadmap/S10/pdb/sound.procs.tsv row 0x{rva:x}: {smsg})").replace("; ;", ";")
        if dead:
            purpose += " Dead in this build: no live code reaches it."
        rows.append(f"{mod}\t{va}\t{name}\taudio\t{evid}\t{purpose}")
        verdict, reason = rule, ""
    elif m:
        name, verdict, pin, reason = "-", m[2], m[3], m[4]
    else:
        kd, det = shape.kind(v["body"])
        body = [b[9:] for b in v["body"] if not b[9:].startswith(("int3", "nop"))]
        pin = nb.get(va, [""] * 6)[5] if va in nb else "-"
        if kd in ("EMPTY", "CONST") or body == ["mov eax, ecx", "ret"]:
            verdict, reason = "REJECT_STUB", det or f"`{'; '.join(body)}` at {va}"
        else:
            verdict = "REJECT_NO_CLASS_PIN"
            c = nb[va]
            reason = (f"dead body; nearest class-carrying named neighbours {c[3]} below and {c[4]} above do not both carry "
                      f"{c[2]} (SMSG class mapped by classmap.py); no vtable store and no call passing this to a named method")
        name = "-"
    cnt[verdict] += 1
    ledger.append(f"{mod}\t{va}\t{smsg}\t{verdict}\t{name}\t{pin}\t{reason}")
(S10 / "13_ledger.tsv").write_text("\n".join(ledger) + "\n", encoding="utf-8")
hdr = "module\taddr\tname\tsubsystem\tevidence\tpurpose\n"
(ROOT / "re/names/smsg_sound_2.tsv").write_text(hdr + "\n".join(rows) + "\n", encoding="utf-8")
print(len(rows), "rows;", dict(sorted(cnt.items())))
