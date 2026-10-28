"""S3.6: the framegate census on S1's six pinned scenarios, which the S3.6 mirror's own scenario list (HEAD c6d7d78b's
re/analysis/data/scenario_refs.json) does not hold: the same run_scenario with the shared repo's scenario list. One TSV row per
checkpoint with the columns framegate writes.

    cd port/build/S3_6_src && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S3/6_census_s1.py <out.tsv> <scenario>...
"""
import csv
import os
import pathlib
import sys

sys.path.insert(0, os.getcwd())
from port.test import framegate as FG  # noqa: E402

SHARED = pathlib.Path(__file__).resolve().parents[3] / "re" / "analysis" / "data" / "scenario_refs.json"


def main():
    out = pathlib.Path(sys.argv[1])
    FG.REFS_JSON = SHARED
    rows = []
    for s in sys.argv[2:]:
        rs = FG.run_scenario(s, probe=False)
        print(s, {r["status"] for r in rs}, len(rs), flush=True)
        rows += rs
    cols = sorted({k for r in rows for k in r})
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        w = csv.DictWriter(f, fieldnames=cols, delimiter="\t", extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow({k: str(r.get(k, "")) for k in cols})


if __name__ == "__main__":
    main()
