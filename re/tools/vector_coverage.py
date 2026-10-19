"""Phase 2.5 coverage statement: per selected function, how much of it the captured vectors exercise.

    py -3.12 re/tools/vector_coverage.py            # -> re/port/vectors_coverage.tsv + summary

Sources: re/port/vectors_manifest.tsv (corpus counts; the function's conditional jumps and the sides the reproduced
vectors took, measured from the emulation's executed edges), the .vec files (per-key open sides), the C3 A/B CSVs
log/diff/<addr>_*.path1.csv (vector rows and distinct_results rows of every key) and the branch-coverage notes of the C3
batches (log/c3/*_purpose.md that cite the address).

Verdict per function (first that applies):
  none            no reproduced vector
  thin            fewer than 10 reproduced vectors, or every reproduced vector gives the same (ret, out)
  partial_branch  a conditional jump of the function no reproduced vector reached on either side
  one_sided       every jump reached, some on one side only
  full            every conditional jump of the function's own body taken both ways (or it has none)
`thin` and `partial_branch` functions need the segment corpus (phase 2.4) for a port test of the parts missed.
"""
from __future__ import annotations

import collections
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

MAN = ROOT / "re" / "port" / "vectors_manifest.tsv"
SEL = ROOT / "re" / "port" / "vector_selection.tsv"
OUT = ROOT / "re" / "port" / "vectors_coverage.tsv"


def c3_csv_facts(addr: int) -> tuple[int, int, int]:
    """(vector rows, sum of distinct_results, CSV files) over the exe's path-1 CSVs of this address."""
    vec = dist = n = 0
    for p in (ROOT / "log" / "diff").glob(f"{addr:08x}_*.path1.csv"):
        stem = p.name[9:]
        if re.match(r"(jgld|sound|Terrain)_", stem):
            continue                      # DLL rows share RVAs; this is an exe address
        n += 1
        for row in csv.reader(p.open(errors="replace")):
            if not row:
                continue
            if row[0] == "distinct_results":
                dist += int(row[1] or 0)
            elif row[0] not in ("vector", "state_regions", "install_witness", "meta", "VERDICT", "error"):
                vec += 1
    return vec, dist, n


def main():
    man = list(csv.DictReader(MAN.open(), delimiter="\t"))
    sel = [r for r in csv.DictReader(SEL.open(), delimiter="\t") if r["selected"] == "1"]
    by_fn = collections.defaultdict(list)
    for r in man:
        by_fn[r["function"]].append(r)
    rows = []
    for s in sel:
        ks = by_fn.get(s["name"], [])
        okk = [k for k in ks if k["status"] in ("ok", "partial")]
        okv = sum(int(k["ok_vectors"]) for k in ks)
        dist = sum(int(k["distinct_results"]) for k in okk)
        cov = ks[0] if ks else {}
        jcc, both, one, none = (int(cov.get(c, 0) or 0) for c in ("fn_jcc", "fn_jcc_both", "fn_jcc_one", "fn_jcc_none"))
        open_sides = cov.get("fn_jcc_open", "-") or "-"
        c3v, c3d, c3n = c3_csv_facts(int(s["addr"], 16))
        notes = sorted({k["branch_notes"] for k in ks if k.get("branch_notes", "-") != "-"})
        if okv == 0:
            verdict = "none"
        elif okv < 10 or dist <= 1:
            verdict = "thin"
        elif none:
            verdict = "partial_branch"
        elif one:
            verdict = "one_sided"
        else:
            verdict = "full"
        why = {k["status"] for k in ks} - {"ok"}
        rows.append({"function": s["name"], "addr": s["addr"], "tier": s["tier"], "in_simcore": s.get("in_simcore", "0"),
                     "keys": len(ks), "keys_reproduced": len(okk), "ok_vectors": okv, "distinct_results": dist,
                     "c3_vectors": c3v, "c3_distinct_results": c3d, "c3_csvs": c3n,
                     "jcc": jcc, "jcc_both": both, "jcc_one_side": one, "jcc_none": none, "open_sides": open_sides,
                     "verdict": verdict, "key_statuses": ",".join(sorted({k["status"] for k in ks})) or "-",
                     "branch_notes": " ".join(notes) or "-"})
    with OUT.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]), delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    c = collections.Counter(r["verdict"] for r in rows)
    print("verdicts:", dict(c))
    print("simcore verdicts:", dict(collections.Counter(r["verdict"] for r in rows if r["in_simcore"] == "1")))
    print(f"-> {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
