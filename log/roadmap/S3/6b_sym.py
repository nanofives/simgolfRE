"""VA -> GameState symbol + offset over re/model/addresses.tsv (top-level rows of golf_clean.exe).

    python log/roadmap/S3/6b_sym.py 0x0059b778 0x005a8c90 ...
"""
import bisect
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
rows = []
for r in csv.DictReader(open(ROOT / "re" / "model" / "addresses.tsv", encoding="utf-8"), delimiter="\t"):
    if r["module"] != "golf_clean.exe" or r["relation"] != "top":
        continue
    rows.append((int(r["va"], 16), int(r["size"], 16), r["symbol"], r["c_type"], r["count"], r["stride"]))
rows.sort()
starts = [r[0] for r in rows]
for a in sys.argv[1:]:
    v = int(a, 16)
    i = bisect.bisect_right(starts, v) - 1
    if i >= 0 and v < rows[i][0] + rows[i][1]:
        va, size, sym, ct, cnt, st = rows[i]
        print(f"{a}\t{sym}+0x{v - va:x}\t(size 0x{size:x}, {ct} x {cnt}, stride {st})")
    else:
        print(f"{a}\t?")
