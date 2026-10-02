"""Spectrum-based fault localization (Ochiai) over INT3 coverage runs.

When a scenario fails with some set of reimplementation hooks ON and passes with them OFF (or passes on
some runs and fails on others), the functions that ran mostly in the failing runs are the suspects:

    ochiai(f) = failed(f) / sqrt(total_failed * (failed(f) + passed(f)))

    py -3.12 re/tools/sbfl.py --pass cov_ok1.tsv cov_ok2.tsv --fail cov_bad1.tsv [--top 20]

Inputs are coverage dumps (module, addr[, mask/phases]) as written by Game.cov_dump() or the census.
Output: ranked suspects, joined with hooks.csv so reimplemented functions (SG_HOOK ON) stand out.
Reference: Abreu et al., Ochiai for SBFL; works only for faults that a failing run executes.
"""
from __future__ import annotations

import argparse
import csv
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]


def load_cov(path) -> set[tuple[str, str]]:
    with open(path, newline="") as f:
        return {(r.get("module") or "golf_clean.exe", r["addr"].lower()) for r in csv.DictReader(f, delimiter="\t")}


def ochiai(passing: list[set], failing: list[set]) -> list[tuple[float, tuple[str, str], int, int]]:
    tf = len(failing)
    elems = set().union(*passing, *failing) if (passing or failing) else set()
    out = []
    for e in elems:
        ef = sum(e in s for s in failing)
        ep = sum(e in s for s in passing)
        score = ef / math.sqrt(tf * (ef + ep)) if tf and ef else 0.0
        out.append((score, e, ef, ep))
    out.sort(key=lambda t: (-t[0], t[1]))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pass", dest="passing", nargs="+", default=[])
    ap.add_argument("--fail", dest="failing", nargs="+", required=True)
    ap.add_argument("--top", type=int, default=20)
    a = ap.parse_args()
    hooks = {}
    if (ROOT / "hooks.csv").exists():
        for r in csv.DictReader(open(ROOT / "hooks.csv", newline="")):
            hooks[(r.get("module") or "golf_clean.exe", r["addr"])] = r
    ranked = ochiai([load_cov(p) for p in a.passing], [load_cov(p) for p in a.failing])
    for score, (mod, addr), ef, ep in ranked[:a.top]:
        h = hooks.get((mod, addr))
        tag = f"  [{h['confidence']} {h['name']}]" if h else ""
        print(f"{score:.3f}  {mod:15} {addr}  failed={ef} passed={ep}{tag}")


if __name__ == "__main__":
    main()
