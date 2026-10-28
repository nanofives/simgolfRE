"""S3.7: GameState symbol + offset of exe data addresses (re/model/addresses.tsv), for writing port code without address literals.

    ~/bin/py -3.12 log/roadmap/S3/7_sym.py 0x4c8270 0x4c26fe ...
"""
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
rows = list(csv.reader(open(ROOT / "re/model/addresses.tsv", encoding="utf-8"), delimiter="\t"))[1:]
iv = [(int(r[2], 16), int(r[3], 16), r[0], r[4]) for r in rows if r[1] == "golf_clean.exe"]
for a in sys.argv[1:]:
    v = int(a, 16)
    m = [(n, hex(v - s), t) for s, o, n, t in iv if s <= v < s + o]
    print(a, m[0] if m else "-")
