"""S10.9 mechanical verdicts over 9_facts_<key>.json.  ~/bin/py -3.12 log/roadmap/S10/9_tools/verdict.py <key>
-> log/roadmap/S10/9_verdict_<key>.tsv: va, verdict (ACCEPT_* | REJECT_* | DEFER_* | MANUAL), smsg name, our name, rule detail."""
import json, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import shape, vtslots  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[4]
key = sys.argv[1]
d = json.loads((ROOT / f"log/roadmap/S10/9_facts_{key}.json").read_text(encoding="utf-8"))
slot_names = vtslots.slots(key)
tabs = vtslots.tables(key)
GET = re.compile(r"^(get|is|has|can|query|num|count|does|Get|Is|Has)(_|[A-Z]|$)")
SET = re.compile(r"^(set|add_to|reset|clear|enable|disable|mute|unmute|switch|toggle|Set|Reset|Clear)(_|[A-Z]|$)")
STORECONST = re.compile(r"^mov (byte|word|dword) ptr \[ecx( \+ 0x[0-9a-f]+)?\], (0x[0-9a-f]+|\d+)$")


def our_name(nm):
    cls, _, meth = nm.rpartition("::")
    if not cls:
        return nm
    base = cls.split("<")[0].split("::")[-1]
    if meth == base:
        meth = "ctor"
    elif meth == "~" + base:
        meth = "dtor"
    elif meth == "`scalar deleting destructor'":
        meth = "scalarDeletingDtor"
    elif meth == "`vector deleting destructor'":
        meth = "vectorDeletingDtor"
    return f"{cls}::{meth}"


import capstone, pointer_callers as pc
_m = pc.mod(vtslots.MODS[key]); _md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def body_of(b, z):
    return [f"{i.address:08x} {i.mnemonic} {i.op_str}".rstrip() for i in _md.disasm(_m.img[b - _m.base:b - _m.base + z], b)]


rows = []
for va, v in d.items():
    nm, meth = v["theirs"], v["theirs"].split("::")[-1]
    kd, det = shape.kind(v["body"])
    pinned = any(r["installers"] for r in v["refs"]) or any("FUN_" not in c for c in v["callers"])
    if v["master"] == "-":
        verdict = "DEFER_NOT_A_FUNCTION"
    elif nm.startswith("$") or "`" in nm and "deleting destructor" not in nm:
        verdict = "MANUAL"
    elif kd in ("EMPTY", "CONST"):
        verdict = "REJECT_STUB"
    elif not pinned:
        verdict = "MANUAL"
    elif kd == "GETTER" and GET.match(meth):
        verdict = "ACCEPT_GET"
    elif kd == "SETTER" and SET.match(meth):
        verdict = "ACCEPT_SET"
    elif SET.match(meth) and v["body"] and all(STORECONST.match(b[9:]) or b[9:].startswith("ret") for b in v["body"]):
        verdict, det = "ACCEPT_SET", "stores constants into this: " + "; ".join(f"`{b[9:]}` 0x{b[:8]}" for b in v["body"][:-1])
    elif kd == "FORWARD":
        s = int(det.split("slot ")[1].split()[0], 16) // 4
        tgt = [b for n, b, z in slot_names.get(s, set()) if n == meth and b != int(va, 16)
               and shape.kind(body_of(b, z))[0] != "FORWARD"]
        verdict = "ACCEPT_FWD" if tgt else "MANUAL"
        det += f"; slot {s} of other SMSG vtables holds a body (not a forwarder) of the same method name: " + (", ".join(f"0x{b:08x}" for b in tgt[:3]) or "none")
    elif kd == "CTOR" and our_name(nm).split("::")[-1] in ("ctor", "dtor", "scalarDeletingDtor", "vectorDeletingDtor"):
        stored = int(re.search(r"stores (0x[0-9a-f]+)", det).group(1), 16)
        pub = tabs.get(stored, ("",))[0]
        cls = nm.rsplit("::", 1)[0]
        ok = pub.startswith(f"??_7{cls.split('<')[0]}@@")
        verdict = "ACCEPT_VT" if ok else "MANUAL"
        det += f"; SMSG's public at that table: {pub or 'none'}"
    elif kd == "SDD" and "scalar deleting" in nm:
        verdict = "ACCEPT_VT"
    else:
        verdict = "MANUAL"
    rows.append(f"{va}\t{verdict}\t{nm}\t{our_name(nm)}\t{kd}\t{det}")
(ROOT / f"log/roadmap/S10/9_verdict_{key}.tsv").write_text("va\tverdict\tsmsg\tname\tkind\tdetail\n" + "\n".join(rows) + "\n", encoding="utf-8")
import collections
print(collections.Counter(r.split("\t")[1] for r in rows))
