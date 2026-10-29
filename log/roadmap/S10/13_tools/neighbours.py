"""S10.13: the named functions on either side of each deferred sound.dll body (our names files only), and the
class prefix each carries. VC6 lays out the functions of one object file contiguously, so a dead body whose nearest
named neighbours below and above both carry our class C sits in the object file that holds C's methods.
~/bin/py -3.12 log/roadmap/S10/13_tools/neighbours.py  -> log/roadmap/S10/13_neighbours.tsv"""
import bisect, json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
S10 = ROOT / "log/roadmap/S10"
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(pathlib.Path(__file__).parent))
import xref, classmap  # noqa: E402

d = json.loads((S10 / "13_facts.json").read_text(encoding="utf-8"))
nm = {a: n[1] for a, n in xref.names("sound.dll").items() if n[0] == "names"}
cm, _ = classmap.classmap("sound")
keys = sorted(nm)


def cls(n):
    # classmap.py's convention: the prefix before `::`, else before the first `_`
    return n.rsplit("::", 1)[0] if "::" in n else (n.split("_")[0] if "_" in n else "-")


rows = ["va\tsmsg\tsmsg_class_ours\tbelow\tabove\tpin"]
for k, v in d.items():
    if v["module"] != "sound.dll":
        continue
    va = int(v["va"], 16)
    i = bisect.bisect_left(keys, va)
    # nearest named neighbour on each side that carries a class prefix (names without one, such as
    # `mapDsError` or `stubReturnBA`, are skipped and listed in the skipped column)
    j, skipped = i - 1, []
    while j > 0 and cls(nm[keys[j]]) == "-":
        skipped.append(nm[keys[j]]); j -= 1
    lo = keys[j] if j >= 0 else None
    j = i
    while j < len(keys) - 1 and cls(nm[keys[j]]) == "-":
        skipped.append(nm[keys[j]]); j += 1
    hi = keys[j] if j < len(keys) else None
    sc = v["smsg"].rsplit("::", 1)[0] if "::" in v["smsg"] else "-"
    ours = cm.get(sc, sc)
    cb, ca = cls(nm[lo]) if lo else "-", cls(nm[hi]) if hi else "-"
    pin = "NEIGHBOUR" if sc != "-" and cb == ca == ours else ("FREE" if sc == "-" else "-")
    rows.append(f"{v['va']}\t{v['smsg']}\t{ours}\t0x{lo:08x} {nm[lo]}\t0x{hi:08x} {nm[hi]}\t{pin}\t{','.join(skipped) or '-'}")
(S10 / "13_neighbours.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")
print(sum(1 for r in rows[1:] if r.split("\t")[5] == "NEIGHBOUR"), "neighbour pins of", len(rows) - 1)
