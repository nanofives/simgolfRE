"""Phase 3.7a: finds, for each negative control of the input wiring and of the tick functions added for the COMMAND
segments, the shortest PASSing COMMAND segment (log/p3/p37a_cmdsegs.tsv) that reads it RED.
usage: ~/bin/py -3.12 log/p3/p37a_negctl_cmd.py [--max 40] [<function>:<mode> ...]"""
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test import segments as S  # noqa: E402

CONTROLS = [("tick.frame", 3), ("tick.frame", 4), ("tick.frame", 5), ("playHabitatSound", 1), ("drawRecords", 1)]


def main():
    args = [a for a in sys.argv[1:] if ":" in a]
    cap = int(sys.argv[sys.argv.index("--max") + 1]) if "--max" in sys.argv else 40
    rows = [r for r in csv.DictReader(open(ROOT / "log/p3/p37a_cmdsegs.tsv", encoding="utf-8"), delimiter="\t")
            if r["status"] == "PASS"]
    rows.sort(key=lambda r: int(r["frames"].split("/")[1]))
    lib = L.load()
    runner = S.SegmentRunner(lib)
    for fn, mode in [c for c in CONTROLS if not args or f"{c[0]}:{c[1]}" in args]:
        found = None
        for r in rows[:cap]:
            sc, k = r["segment"].split("/")
            seg = S.load_segment(S.SEG_DIR / sc / k)
            lib.negctl(fn, mode)
            try:
                res = runner.run(seg)
            finally:
                lib.negctl_reset()
            if res.status != "PASS":
                found = (r["segment"], res.status, res.detail[:120])
                break
        print(fn, mode, "RED on" if found else f"BLIND on the first {cap}", found or "", flush=True)


if __name__ == "__main__":
    main()
