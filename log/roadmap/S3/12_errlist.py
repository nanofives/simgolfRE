"""S3.12: the distinct compile / link errors of a build log of 12_build.sh (file(line,col): error ...), in log order.

    ~/bin/py -3.12 log/roadmap/S3/12_errlist.py <tree> <arch> [...]
"""
import re
import sys
from pathlib import Path

D = Path(__file__).resolve().parent
args = sys.argv[1:]
for tree, arch in zip(args[::2], args[1::2]):
    rows = []
    pat = re.compile(re.escape(tree) + r"[\\/]port[\\/](\S+?\(\d+,\d+\)): (error \w+: .*?) \[")
    for line in open(D / f"12_build_{tree}_{arch}.log", encoding="utf-8", errors="replace"):
        m = pat.search(line)
        if m:
            rows.append(f"{m.group(1)}: {m.group(2)}")
        elif re.search(r"error (LNK|MSB)\d+", line):
            rows.append(line.strip()[:220])
    rows = list(dict.fromkeys(rows))
    print(f"== {tree} {arch}: {len(rows)} distinct errors")
    for r in rows:
        print("  " + r)
