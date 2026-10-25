"""S2.4 helper: map golf_clean.exe data VAs to (model symbol + offset) from re/model/addresses.tsv, and print the C string at
each VA read from original/golf_clean.exe (for checking only; the port names strings by pool offset, never by text).

usage: ~/bin/py -3.12 log/roadmap/S2/4_tools/va2sym.py 0x4d3600 0x4c591c ...
"""
import csv
import pathlib
import sys

import pefile

ROOT = pathlib.Path(__file__).resolve().parents[4]


def load():
    rows = []
    with open(ROOT / "re" / "model" / "addresses.tsv", newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["module"] != "golf_clean.exe":
                continue
            rows.append((int(r["va"], 16), int(r["size"], 16), r["symbol"]))
    rows.sort()
    return rows


def main():
    rows = load()
    pe = pefile.PE(str(ROOT / "original" / "golf_clean.exe"), fast_load=True)
    for a in sys.argv[1:]:
        va = int(a, 16)
        hit = [s for s in rows if s[0] <= va < s[0] + s[1]]
        try:
            raw = pe.get_data(va - pe.OPTIONAL_HEADER.ImageBase, 64)
            txt = raw.split(b"\0", 1)[0].decode("latin-1")
        except Exception:
            txt = "?"
        if hit:
            b, sz, sym = hit[-1]
            print(f"0x{va:08x}\t{sym}+0x{va - b:x}\t{txt!r}")
        else:
            print(f"0x{va:08x}\t-\t{txt!r}")


if __name__ == "__main__":
    main()
