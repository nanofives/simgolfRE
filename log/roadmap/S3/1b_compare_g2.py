"""S3.1b: per-pass G2 numbers of two 1_g2.py logs (before: S3.1's 1_g2_all.log, after: 1b_g2_all.log), split g6 / the rest.
    ~/bin/py -3.12 log/roadmap/S3/1b_compare_g2.py log/roadmap/S3/1_g2_all.log log/roadmap/S3/1b_g2_all.log"""
import re
import sys

PAT = re.compile(r"^(\S+) (\S+) kind=(\d) drawn=(\d+) changed=(\d+) one=(\d+) \(([\d.]+)%\) big=(\d+) (OK|FAIL)")


def load(p):
    out = {}
    for ln in open(p, encoding="utf-8"):
        m = PAT.match(ln)
        if m:
            out[(m[1], m[2])] = (int(m[4]), int(m[6]), float(m[7]), int(m[8]), m[9])
    return out


a, b = load(sys.argv[1]), load(sys.argv[2])
for grp, sel in (("g6", lambda k: k[0] == "g6"), ("other", lambda k: k[0] != "g6")):
    ks = [k for k in a if sel(k) and k in b]
    drawn = [k for k in ks if a[k][0] > 0]
    worse = [k for k in drawn if b[k][2] > a[k][2]]
    print(f"{grp}: passes {len(ks)} (drawing {len(drawn)}) OK before {sum(a[k][4] == 'OK' for k in ks)} after {sum(b[k][4] == 'OK' for k in ks)}")
    print(f"   mean one-level % before {sum(a[k][2] for k in drawn) / len(drawn):.3f} after {sum(b[k][2] for k in drawn) / len(drawn):.3f}; "
          f"one-level px {sum(a[k][1] for k in ks)} -> {sum(b[k][1] for k in ks)}; big px {sum(a[k][3] for k in ks)} -> {sum(b[k][3] for k in ks)}")
    print(f"   max one-level % before {max(a[k][2] for k in drawn):.2f} after {max(b[k][2] for k in drawn):.2f}; max big before {max(a[k][3] for k in ks)} after {max(b[k][3] for k in ks)}")
    print(f"   passes with a higher one-level %: {len(worse)}", [f"{k[0]}/{k[1]} {a[k][2]}->{b[k][2]}" for k in worse][:10])
