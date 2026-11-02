"""P3: committed private regions (>= 64 KB, SG_P3_REGIONS rows: frame, allocation base, base, size, protect) of one run,
compared between two samples.

    ~/bin/py -3.12 log/roadmap/P3/regdiff.py <run> [frame_a] [frame_b] [--occ k]

frame_b defaults to the last sample; samples sharing a frame number (the pre-teardown / after-audio / end samples) are told
apart with --occ (0 = first sample with that frame, -1 = last). Prints regions only in a, only in b, and regions whose size
changed (same base), with the byte totals.
"""
import pathlib
import sys
from collections import defaultdict

RUNS = pathlib.Path(__file__).resolve().parent / "runs"


def samples(run):
    out = []  # list of (frame, {base: (alloc, size, prot)})
    prev = None
    for line in (RUNS / run / "regions.tsv").read_text().splitlines():
        f, ab, b, sz, pr = line.split("\t")
        f = int(f)
        # a new sample starts when the frame changes or the base addresses restart (lower than the previous row)
        bi = int(b, 16)
        if prev is None or f != prev[0] or bi <= prev[1]:
            out.append((f, {}))
        out[-1][1][bi] = (int(ab, 16), int(sz), pr)
        prev = (f, bi)
    return out


def main(argv):
    occ = 0
    if "--occ" in argv:
        i = argv.index("--occ")
        occ = int(argv[i + 1])
        del argv[i:i + 2]
    run = argv[0]
    s = samples(run)
    fa = int(argv[1]) if len(argv) > 1 else 2000
    a = [m for f, m in s if f == fa][0]
    if len(argv) > 2:
        b = [m for f, m in s if f == int(argv[2])][occ]
    else:
        b = s[-1][1]
    only_a = {k: v for k, v in a.items() if k not in b}
    only_b = {k: v for k, v in b.items() if k not in a}
    changed = {k: (a[k], b[k]) for k in a if k in b and a[k][1] != b[k][1]}
    print(f"samples {len(s)}; a={fa} total {sum(v[1] for v in a.values()):,}  b total {sum(v[1] for v in b.values()):,}")
    print(f"only in a: {len(only_a)} regions {sum(v[1] for v in only_a.values()):,} B")
    for k, v in sorted(only_a.items()):
        print(f"   - {k:#x} alloc {v[0]:#x} {v[1]:,} {v[2]}")
    print(f"only in b: {len(only_b)} regions {sum(v[1] for v in only_b.values()):,} B")
    for k, v in sorted(only_b.items()):
        print(f"   + {k:#x} alloc {v[0]:#x} {v[1]:,} {v[2]}")
    print(f"size changed: {len(changed)} net {sum(y[1] - x[1] for x, y in changed.values()):+,} B")
    for k, (x, y) in sorted(changed.items()):
        print(f"   ~ {k:#x} alloc {x[0]:#x} {x[1]:,} -> {y[1]:,} ({y[1] - x[1]:+,})")


if __name__ == "__main__":
    main(sys.argv[1:])
