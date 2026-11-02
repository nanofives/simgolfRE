"""S11.2 information: master names (export) that no re/names row backs, per module and source.

usage: 2_unbacked.py <names_dir> <export_dir> <out.tsv>
A function is backed when a function-file row names its address; a label when a *_globals.tsv row names its address.
"""
import csv
import os
import sys

names, export, out = sys.argv[1:4]
MODS = ["golf_clean.exe", "jgld.dll", "Terrain.dll", "sound.dll"]
fn, gl = set(), set()
for f in sorted(os.listdir(names)):
    if not f.endswith(".tsv") or f == "withdrawn.tsv":
        continue
    for ln in open(os.path.join(names, f), encoding="utf-8").read().splitlines()[1:]:
        c = ln.split("\t")
        if len(c) > 2 and c[0] in MODS and c[2]:
            (gl if f.endswith("_globals.tsv") else fn).add((c[0], int(c[1], 16)))
counts = {}
with open(out, "w", encoding="utf-8", newline="") as h:
    w = csv.writer(h, delimiter="\t", lineterminator="\n")
    w.writerow(["module", "kind", "addr", "name", "source"])
    for m in MODS:
        with open(os.path.join(export, m + ".tsv"), encoding="utf-8") as e:
            r = csv.reader(e, delimiter="\t", quoting=csv.QUOTE_NONE)
            next(r)
            for kind, addr, name, src, primary, st in r:
                a = int(addr, 16)
                if src == "DEFAULT":
                    continue
                if kind == "FN" and (m, a) in fn:
                    continue
                if kind == "SYM" and ((m, a) in gl or primary != "1"):
                    continue
                k = (m, kind, src)
                counts[k] = counts.get(k, 0) + 1
                if src == "USER_DEFINED":
                    w.writerow([m, kind, "0x%08x" % a, name, src])
for k in sorted(counts):
    print("%-15s %-4s %-13s %d" % (k[0], k[1], k[2], counts[k]))
