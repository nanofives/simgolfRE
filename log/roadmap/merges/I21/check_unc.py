"""I21: every U- row of the mirror's UNCERTAINTIES.md has 6 cells (escaped pipes ignored)."""
import re
import sys

bad = 0
for l in open(sys.argv[1], encoding="utf-8"):
    if l.startswith("| U-"):
        n = len(re.split(r"(?<!\\)\|", l.rstrip("\r\n")))
        if n != 8:
            bad += 1
            print("BAD", l[:12], n)
print("checked, bad =", bad)
