"""S3.12 copy of 10_equiv.py (tree = $SG_TREE, default port/build/S3_12; mode `workerctl` = `#threads 4` + `#workerctl 1`,
the workers' negative control). S3.10: the fast pixel loop against the general loop on every recorded GL pass (port/corpus/gl/<label>, plus t3 when present):
each snapshot replayed (port/test/glraster.run: the original's state stream, starting buffer and pass stream) with
  general  `#fastpath 0`  the general loop of GlRaster::triangle (the rasteriser as before S3.10)
  fast     (default)      the fast loop with the checked reciprocal path
  exact    `#fastpath 2`  the fast loop with the exact expressions only
  threads  `#threads 4`   the fast loop on 4 workers
  sloppy   `#fastpath 3`  NEGATIVE CONTROL: the reciprocal path without its boundary checks
  crude    `#fastpath 4`  NEGATIVE CONTROL: reciprocals rounded to float, checks kept
and every frame and pixel counter compared with the general loop's. Also the chain path (sg_gl_render_to_raster: the port's
Terrain::render drives the rasteriser, no stream) for the render-entry snapshots, general against fast + 4 workers.

    SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 log/roadmap/S3/10_equiv.py [label ...] [--out <tsv>]
"""
import argparse
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
TREE = pathlib.Path(os.environ.get("SG_TREE") or ROOT / "port" / "build" / "S3_12")   # S3.12: the tree is a parameter
sys.path.insert(0, str(TREE))

import numpy as np  # noqa: E402

from port.test import glcorpus as G  # noqa: E402
from port.test import glraster as R  # noqa: E402
from port.test import lib as L  # noqa: E402

MODES = {"general": "#fastpath 0\n", "fast": "", "exact": "#fastpath 2\n", "threads": "#threads 4\n", "sloppy": "#fastpath 3\n",
         "crude": "#fastpath 4\n", "workerctl": "#threads 4\n#workerctl 1\n"}
T3 = ROOT / "log" / "p5" / "gl" / "corpus" / "t3"


def roots(labels):
    out = []
    for lab in labels or sorted(p.name for p in G.CORPUS.iterdir() if (p / "snaps").is_dir()):
        p = T3 if lab == "t3" else G.CORPUS / lab
        if (p / "snaps").is_dir() and G.snaps(p):
            out.append((lab, p))
    if not labels and (T3 / "snaps").is_dir():
        out.append(("t3", T3))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("labels", nargs="*")
    ap.add_argument("--out")
    a = ap.parse_args()
    lib = L.load(os.environ.get("SG_PORTSIM_DLL"))
    rows, bad, ctl, n = [], [], {"sloppy": 0, "crude": 0, "workerctl": 0}, 0
    for lab, root in roots(a.labels):
        setup = R.Setup(root)
        for step in G.snaps(root):
            res = {}
            for mode, line in MODES.items():
                def patch(r, dll, line=line):
                    if line:
                        dll.sg_raster_exec(r, line.encode(), len(line))
                post, got, st, unknown, pre = R.run(lib, setup, step, patch=patch)
                res[mode] = (got.copy(), st)
            ref, rst = res["general"]
            line = [lab, step, str(rst[2])]
            for mode in ("fast", "exact", "threads", "sloppy", "crude", "workerctl"):
                got, st = res[mode]
                px = int((got != ref).sum())
                same_stats = st[:5] == rst[:5]
                line.append(f"{px}{'' if same_stats else '*'}")
                if mode in ("sloppy", "crude", "workerctl"):
                    ctl[mode] += px > 0
                elif px or not same_stats:
                    bad.append((lab, step, mode, px, st, rst))
            chain = ""
            if G.meta(root / "snaps" / step / "meta.txt").get("entry_kind") == G.ENTRY_RENDER and lab != "t3":
                def p0(r, dll):
                    dll.sg_raster_exec(r, b"#fastpath 0\n", 12)

                def p4(r, dll):
                    dll.sg_raster_exec(r, b"#threads 4\n", 11)
                _, g0, s0, _ = R.run_chain(lib, setup, step, patch=p0)
                _, g4, s4, _ = R.run_chain(lib, setup, step, patch=p4)
                cpx = int((g0 != g4).sum())
                chain = f"{cpx}{'' if s0[:5] == s4[:5] else '*'}"
                if cpx or s0[:5] != s4[:5]:
                    bad.append((lab, step, "chain", cpx, s4, s0))
            line.append(chain)
            rows.append("\t".join(line))
            n += 1
            print("\t".join(line), flush=True)
    print(f"passes {n}: fast / exact / threads / chain differing from the general loop: {len(bad)}; sloppy control differs on "
          f"{ctl['sloppy']}, crude control on {ctl['crude']}, workerctl control on {ctl['workerctl']}", flush=True)
    for b in bad[:20]:
        print("BAD", b[:4], flush=True)
    if a.out:
        pathlib.Path(a.out).write_text("label\tstep\tdrawn\tfast_px\texact_px\tthreads_px\tsloppy_px\tcrude_px\tworkerctl_px\tchain_px\n" + "\n".join(rows) + "\n")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
