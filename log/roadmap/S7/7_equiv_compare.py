"""S7.7: compare the rebased equivalence TSVs (7_equiv_x64.tsv, 7_equiv_w32.tsv) with S7.6's on c6d7d78b (6_equiv_x64.tsv) and with
each other: status / records / expected / boot_status / replay / violations per scenario, the new rows (S1 / S8 recordings) listed.

    ~/bin/py -3.12 log/roadmap/S7/7_equiv_compare.py            (from the shared repo root)
"""
import collections
import csv
import pathlib
import sys

D = pathlib.Path("log/roadmap/S7")
KEYS = ("status", "records", "expected", "boot_status", "live_vs_boot", "replay_vs_live", "violations")


def rd(p):
    return {r["scenario"]: r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")} if p.exists() else {}


old, x64, w32 = rd(D / "6_equiv_x64.tsv"), rd(D / "7_equiv_x64.tsv"), rd(D / "7_equiv_w32.tsv")
# single-scenario re-runs (7_equiv_<backend>_<scenario>.tsv) replace that row of the full run; each is listed
for tag, new in (("x64", x64), ("w32", w32)):
    for f in sorted(D.glob(f"7_equiv_{tag}_*.tsv")):
        for s, r in rd(f).items():
            print(f"re-run {f.name}: {s} {(new.get(s) or {}).get('status')} -> {r['status']}")
            new[s] = r
bad = 0
for name, new in (("x64", x64), ("win32", w32)):
    if not new:
        print(name, "no TSV")
        continue
    print(name, len(new), "rows", dict(collections.Counter(r["status"] for r in new.values())),
          "violations", sum(int(r["violations"] or 0) for r in new.values()),
          "replay differences", sum(1 for r in new.values() if r["replay_vs_live"]))
    for s in sorted(new):
        if s not in old:
            r = new[s]
            print("  new row", s, r["status"], r["records"], r["expected"], "boot", r["boot_status"], "viol", r["violations"],
                  "replay", "equal" if not r["replay_vs_live"] else r["replay_vs_live"][:80])
            continue
        d = [k for k in ("status", "records", "expected", "boot_status") if old[s][k] != new[s][k]]
        if d:
            bad += 1
            print("  DIFF vs S7.6", s, {k: (old[s][k], new[s][k]) for k in d})
    for s in sorted(set(old) - set(new)):
        print("  missing", s)
if x64 and w32:
    d = [s for s in sorted(set(x64) | set(w32)) if any((x64.get(s) or {}).get(k) != (w32.get(s) or {}).get(k) for k in KEYS)]
    print("x64 vs win32: rows differing", len(d), d[:10])
    bad += len(d)
sys.exit(1 if bad else 0)
