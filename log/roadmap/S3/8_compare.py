"""S3.8 regression comparisons: the segment tables (x64, Win32) against the committed tree's (log/roadmap/merges/S2S3-5/segs_*.tsv)
and against each other, and the boot gate TSV row for row against the committed tree's (log/roadmap/merges/S2S3-5/boot_x64.tsv), with
the counts over HEAD's 61 scenarios (log/p5/boot_gate_loc.tsv).

    ~/bin/py -3.12 log/roadmap/S3/8_compare.py
"""
import collections
import csv
import pathlib

S = pathlib.Path("log/roadmap/S3")
M = pathlib.Path("log/roadmap/merges/S2S3-5")


def rd(p, key):
    return {r[key]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


bad = 0
segs = {}
for b in ("x64", "win32"):
    p = S / f"8_segs_{b}.tsv"
    if not p.exists():
        print("missing", p)
        continue
    ref, new = rd(M / f"segs_{b}.tsv", "name"), rd(p, "name")
    segs[b] = new
    d = [n for n in ref if n not in new or ref[n]["digest"] != new[n]["digest"] or new[n]["status"] != ref[n]["status"]]
    print(f"committed {b} vs 8 {b}", len(ref), len(new), "differ", len(d), d[:5])
    bad += len(d)
if len(segs) == 2:
    d = [n for n in segs["x64"] if segs["x64"][n]["digest"] != segs["win32"].get(n, {}).get("digest")]
    print("8 x64 vs 8 win32", len(segs["x64"]), len(segs["win32"]), "differ", len(d))
    bad += len(d)
p = S / "8_boot_gate.tsv"
if p.exists():
    head, base, new = rd("log/p5/boot_gate_loc.tsv", "scenario"), rd(M / "boot_x64.tsv", "scenario"), rd(p, "scenario")
    nd = 0
    for n in sorted(base):
        w = new.get(n)
        if w is None or any(base[n][k] != w[k] for k in ("status", "records", "expected")):
            nd += 1
            print("BOOT DIFF", n, None if w is None else {k: (base[n][k], w[k]) for k in ("status", "records", "expected")})
    print("boot: committed rows", len(base), "new rows", len(new), "differ", nd,
          "HEAD-61 counts", dict(collections.Counter(new[n]["status"] for n in head if n in new)))
    bad += nd
print("total differences:", bad)
