"""S3.1b: per-corpus one-level / big numbers of two 1b_g2_stream.py logs (before = S3.1's DLL, after = S3.1b's).
    ~/bin/py -3.12 log/roadmap/S3/1b_compare_stream.py log/roadmap/S3/1b_g2_stream_before.log log/roadmap/S3/1b_g2_stream.log"""
import re
import sys
from collections import defaultdict

PAT = re.compile(r"^(\S+) (\S+) drawn=(\d+) one=(\d+) \(([\d.]+)%\) big=(\d+) unk=\d+ (OK|FAIL)")


def load(p):
    return {(m[1], m[2]): (int(m[3]), int(m[4]), float(m[5]), int(m[6]), m[7]) for m in map(PAT.match, open(p, encoding="utf-8")) if m}


a, b = load(sys.argv[1]), load(sys.argv[2])
by = defaultdict(list)
for k in a:
    if k in b:
        by[k[0]].append(k)
for c, ks in sorted(by.items()):
    worse = [k[1] for k in ks if b[k][2] > a[k][2]]
    print(f"{c}: {len(ks)} passes, OK {sum(a[k][4] == 'OK' for k in ks)} -> {sum(b[k][4] == 'OK' for k in ks)}, mean one-level % "
          f"{sum(a[k][2] for k in ks) / len(ks):.3f} -> {sum(b[k][2] for k in ks) / len(ks):.3f}, max {max(a[k][2] for k in ks):.2f} -> "
          f"{max(b[k][2] for k in ks):.2f}, big px {sum(a[k][3] for k in ks)} -> {sum(b[k][3] for k in ks)} (max {max(a[k][3] for k in ks)} -> "
          f"{max(b[k][3] for k in ks)}), higher one-level: {worse}")
