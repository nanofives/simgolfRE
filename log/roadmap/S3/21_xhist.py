"""S3.21: where in x the GlRaster stream replays of a corpus differ from the original's dib_post (one-level px), per 16-px column group.

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/21_xhist.py [corpus] [step ...]
Prints per step: one-level / beyond counts, and the counts per 16-px group of x (x // 16) and per y // 16 group (top 12).
"""
import collections
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test import glraster as R  # noqa: E402


def ch(v):
    return np.stack([((v >> s) & 0x1f).astype(np.int16) for s in (10, 5, 0)], axis=-1)


def main():
    args = sys.argv[1:]
    corpus = args[0] if args else "c7"
    root = ROOT / "port" / "corpus" / "gl" / corpus
    setup = R.Setup(root)
    steps = args[1:] or sorted(p.name for p in (root / "snaps").iterdir())
    lib = L.load()
    totx = collections.Counter()
    tot1 = totb = 0
    for s in steps:
        post, got, st, unk, pre = R.run(lib, setup, s)
        d = np.abs(ch(got.reshape(600, 800)) - ch(post.reshape(600, 800))).max(-1)
        ys, xs = np.nonzero(d == 1)
        n1, nb = len(xs), int((d > 1).sum())
        tot1 += n1
        totb += nb
        cx = collections.Counter(int(x) // 16 for x in xs)
        totx.update(cx)
        top = sorted(cx.items(), key=lambda kv: -kv[1])[:10]
        print(f"{s:20s} one {n1:6d} beyond {nb:4d}  xgroups {top}", flush=True)
    print("TOTAL one", tot1, "beyond", totb)
    print("x16 groups:", sorted(totx.items()))


if __name__ == "__main__":
    main()
