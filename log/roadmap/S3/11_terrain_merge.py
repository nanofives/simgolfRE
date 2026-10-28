"""S3.11: the terrain verdicts of the full run (11_terrain_verdicts.tsv) with cov_k3_build taken from its single-scenario re-run
(11_terrain_k3build.tsv; the full run's boot of that scenario failed once with no recorded reason, two re-runs PASS 12 / 12 with
I6's numbers), written to 11_terrain_verdicts_final.tsv, then every row compared with I6's (log/roadmap/merges/I6/terrain_verdicts.tsv).
    ~/bin/py -3.12 log/roadmap/S3/11_terrain_merge.py > log/roadmap/S3/11_terrain_merge.log"""
import collections
import csv

S = "log/roadmap/S3/"


def rows(p):
    with open(p, encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


full, rerun = rows(S + "11_terrain_verdicts.tsv"), {(r["scenario"], r["checkpoint"]): r for r in rows(S + "11_terrain_k3build.tsv")}
out = [rerun.get((r["scenario"], r["checkpoint"]), r) for r in full]
with open(S + "11_terrain_verdicts_final.tsv", "w", encoding="utf-8", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(full[0].keys()), delimiter="\t", lineterminator="\n")
    w.writeheader()
    w.writerows(out)
print("final verdicts:", dict(collections.Counter(r["verdict"] for r in out)))
i6 = {(r["scenario"], r["checkpoint"]): r for r in rows("log/roadmap/merges/I6/terrain_verdicts.tsv")}
same = diff = 0
for r in out:
    o = i6.get((r["scenario"], r["checkpoint"]))
    keys = ("frame", "drawn", "diff", "one", "big", "verdict")
    if o and all(o[k] == r[k] for k in keys):
        same += 1
    else:
        diff += 1
        print("differs:", r["scenario"], r["checkpoint"], {k: (o[k] if o else None, r[k]) for k in keys if not o or o[k] != r[k]})
print(f"rows equal to I6 in frame / drawn / diff / one / big / verdict: {same} of {len(out)}, differ {diff}")
