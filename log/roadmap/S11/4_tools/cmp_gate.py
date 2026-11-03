"""S11.4: compare a segment / boot gate table of the renamed copy with I22's, row for row.

    ~/bin/py -3.12 log/roadmap/S11/4_tools/cmp_gate.py <I22 tsv> <S11_4 tsv>

Every column is compared except `seconds`; text columns (run_detail, detail) are compared after the old names of 4_renames.tsv
are replaced by the new ones in I22's text (they name the function a run stopped in). Prints the differing rows and a summary.
"""
from __future__ import annotations

import csv
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import lexrename as LX  # noqa: E402

ROWS = list(csv.DictReader(open(HERE.parent / "4_renames.tsv", encoding="utf-8"), delimiter="\t"))
PAIRS = [(r["old"], r["new"]) for r in ROWS]


def load(p):
    with open(p, newline="", encoding="utf-8") as f:
        rs = list(csv.DictReader(f, delimiter="\t"))
    key = "name" if "name" in rs[0] else "scenario"
    return {r[key]: r for r in rs}, key


def main() -> None:
    a, ka = load(sys.argv[1])
    b, _ = load(sys.argv[2])
    diff = 0
    for k in sorted(set(a) | set(b)):
        ra, rb = a.get(k), b.get(k)
        if ra is None or rb is None:
            print("ROW MISSING", k, "in I22" if ra is None else "in S11_4")
            diff += 1
            continue
        for c in ra:
            if c == "seconds":
                continue
            va, vb = ra[c], rb.get(c, "")
            if c in ("run_detail", "detail"):
                va = LX.word_sub(va, PAIRS)[0]
            if va != vb:
                print(f"DIFF {k} {c}: I22 {va[:160]!r} | S11_4 {vb[:160]!r}")
                diff += 1
    st = {}
    for r in b.values():
        st[r["status"]] = st.get(r["status"], 0) + 1
    print(f"rows {len(b)} ({len(a)} in I22), statuses {st}, differing cells {diff}")
    sys.exit(1 if diff else 0)


if __name__ == "__main__":
    main()
