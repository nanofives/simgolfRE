"""S10.10 verdicts for the 204 exe rows of 10_facts_exe.json. Rules in order:
ICF (> 1 distinct golf.pdb name at the address, U-1850) -> REJECT_ICF; hand table 10_manual_exe.tsv; forwarders of
10_fwd.tsv -> ACCEPT_FWD when every reached body has a names row whose method equals golf.pdb's (case and `_` ignored,
address suffix dropped), else REJECT_FWD_STUB / _UNNAMED / _CONFLICT; constant / empty bodies -> REJECT_STUB; this-adjustor
and vtordisp thunks follow their target. ~/bin/py -3.12 log/roadmap/S10/10_tools/verdict_exe.py -> 10_verdict_exe.tsv"""
import json, pathlib, re, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from shape import kind  # noqa: E402
from rows import name_rows, demangled  # noqa: E402
R = pathlib.Path(__file__).resolve().parents[4]
S = R / "log/roadmap/S10"
facts = json.loads((S / "10_facts_exe.json").read_text(encoding="utf-8"))
hand = {c[0]: c for c in (l.split("\t") for l in (S / "10_manual_exe.tsv").read_text(encoding="utf-8").splitlines()[1:])}
fwd = {c[0]: c for c in (l.split("\t") for l in (S / "10_fwd.tsv").read_text(encoding="utf-8").splitlines()[1:])}
STUBS = {"stubReturnBA", "stubReturnBB"}
LEDGER = {c[0]: c for c in (l.split("	") for l in (S / "9_ledger_sound.tsv").read_text(encoding="utf-8").splitlines()[1:])}


def norm(n):
    m = n.split("::")[-1] if "::" in n else (n.split("_", 1)[1] if "_" in n and n[0].isupper() else n)
    return re.sub(r"_[0-9a-f]{8}$", "", m).replace("_", "").lower()


def camel(s):
    p = s.split("_")
    return p[0] + "".join(x[:1].upper() + x[1:] for x in p[1:])


def fwd_verdict(a, v):
    c = fwd[a]
    th, tnames, tg = v["theirs"], c[8].split(","), c[7]
    if any(t in STUBS for t in tnames):
        return "REJECT_FWD_STUB", "-", f"slot {c[4]} of [this+{c[3]}] reaches the constant stub {c[8]} ({tg})"
    if any(t == "-" for t in tnames):
        return "REJECT_FWD_UNNAMED", "-", f"slot {c[4]} of [this+{c[3]}] (call {c[5]}, vtables {c[6]}) reaches {tg}, which has no names row (S10.9 ledger: " + "; ".join(f"{t} {LEDGER[t][1]} {LEDGER[t][4]}" for t in tg.split(",") if t in LEDGER) + ")"
    m = th.split("::")[-1]
    ev = f"`mov ecx, [ecx+{c[3]}]` then slot {c[4]} ({c[5]}) of vtable(s) {c[6]} -> {tg} = {c[8]} (sound.pdb {c[9]})"
    if all(norm(t) == m.replace("_", "").lower() for t in tnames):
        return "ACCEPT_FWD", v["ours"].split("::")[0] + "::" + camel(m), ev
    return "REJECT_FWD_CONFLICT", "-", ev + f"; golf.pdb says {m}, U-1896"


def thunk_target(v):
    ins = [b.split(" ", 2) for b in v["body"]]
    j = [x for x in ins if x[1] == "jmp" and len(x) == 3 and x[2].startswith("0x")]
    if j and all(x[1] in ("sub", "jmp") for x in ins[:ins.index(j[0]) + 1]):
        return "0x%x" % int(j[0][2], 16)
    return None


def verdicts():
    out = {}
    order = sorted(facts, key=lambda a: (thunk_target(facts[a]) is not None, int(a, 16)))
    for a in order:
        v = facts[a]
        dn = demangled(v)
        if len(dn) > 1:
            out[a] = ("REJECT_ICF", "-", f"{len(dn)} golf.pdb names share the address ({', '.join(dn[:4])}...): an ICF-folded body keeps a neutral name (U-1850)")
        elif a in hand:
            out[a] = (hand[a][1], hand[a][2], hand[a][3])
        elif a in fwd:
            out[a] = fwd_verdict(a, v)
        elif kind(v["body"])[0] in ("EMPTY", "CONST") or v["body"][0].endswith("mov eax, ecx"):
            out[a] = ("REJECT_STUB", "-", f"{kind(v['body'])[1] or v['body'][0]}: no instruction supports golf.pdb's verb")
        elif thunk_target(v):
            t = thunk_target(v)
            tv = out.get(t)
            if tv and tv[0].startswith("ACCEPT"):
                old = facts[t]["ours"]
                new = v["ours"].replace(old, tv[1]) if old in v["ours"] else tv[1] + "_thunk"
                out[a] = ("ACCEPT_THUNK", new, f"this-adjustor thunk ({'; '.join(v['body'][:2])}) to {t} {old}, renamed {tv[1]}")
            else:
                out[a] = ("REJECT_THUNK", "-", f"this-adjustor thunk ({'; '.join(v['body'][:2])}) to {t}, whose name is kept ({tv[0] if tv else 'outside this set'})")
        else:
            out[a] = ("UNDECIDED", "-", "")
    return out


if __name__ == "__main__":
    rows = name_rows()
    res = verdicts()
    taken = {r[1][2] for va, r in rows.items() if "0x%x" % va not in res or not res["0x%x" % va][0].startswith("ACCEPT")}
    news = [r[1] for r in res.values() if r[0].startswith("ACCEPT")]
    for a, (v, n, e) in list(res.items()):
        if v.startswith("ACCEPT") and (n in taken or news.count(n) > 1):
            res[a] = (v, f"{n}_{a[2:]}", e + f"; `{n}` is taken or proposed twice, so the address is appended")
    lines = ["addr\tfile\tours\ttheirs\tverdict\tnew\tevidence"]
    for a in sorted(res, key=lambda x: int(x, 16)):
        f = rows.get(int(a, 16), ("NOROW",))[0]
        v, n, e = res[a]
        lines.append(f"{a}\t{f}\t{facts[a]['ours']}\t{facts[a]['theirs']}\t{v}\t{n}\t{e}")
    (S / "10_verdict_exe.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")
    import collections
    print(collections.Counter(r[0] for r in res.values()))
