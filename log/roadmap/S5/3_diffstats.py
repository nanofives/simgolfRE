"""S5.3: the FreeType raster's remaining difference from GDI, per font: jobs equal, ink pixels (pixels GDI changed from the
background), differing pixels and their share of the ink, and the differing pixels by their largest channel error in 5-bit
steps (1, 2-3, 4-7, 8+). Reads port/corpus/labels/rasters/freetype_x64.npz and gdi_x64.npz.
    ~/bin/py -3.12 log/roadmap/S5/3_diffstats.py [--provider freetype] [--out <tsv>]"""
import argparse
import collections
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import labels as LB  # noqa: E402

NAMES = {0x519928: "Manual SSi Bold 20", 0x519948: "Manual SSi Bold 24", 0x519a40: "Klepto ITC TT 24",
         0x519fd8: "Arial Bold 10 (arial.ttf)", 0x51a028: "Klepto ITC TT 18", 0x51b360: "Manual SSi Bold 15",
         0x821020: "Klepto ITC TT 24", 0x821ee8: "Manual SSi 14", 0x821f28: "Manual SSi 16"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--provider", default="freetype")
    ap.add_argument("--out")
    a = ap.parse_args()
    corpus = LB.load_corpus()
    ents = corpus["entries"]
    ft = LB.load_rasters(LB.OUT / "rasters" / f"{a.provider}_x64.npz")
    gd = LB.load_rasters(LB.OUT / "rasters" / "gdi_x64.npz")
    S = collections.defaultdict(lambda: collections.Counter())
    for (cfg, eid, c, bg, sh), x, y in zip(corpus["jobs"], ft, gd):
        f = ents[eid]["fontref"]
        s = S[f]
        s["jobs"] += 1
        ink = y != bg
        d = x != y
        s["ink"] += int(ink.sum())
        s["ft_ink"] += int((x != bg).sum())
        s["differ"] += int(d.sum())
        s["equal_jobs"] += 0 if d.any() else 1
        if d.any():
            e = np.max(np.abs(np.stack([(x >> k & 31).astype(int) - (y >> k & 31).astype(int) for k in (10, 5, 0)])), 0)[d]
            s["e1"] += int((e == 1).sum())
            s["e2_3"] += int(((e >= 2) & (e <= 3)).sum())
            s["e4_7"] += int(((e >= 4) & (e <= 7)).sum())
            s["e8"] += int((e >= 8).sum())
    rows = ["fontref\tface\tjobs\tjobs_equal\tgdi_ink_px\tft_ink_px\tpx_differ\tdiffer_per_ink\terr1\terr2_3\terr4_7\terr8plus"]
    tot = collections.Counter()
    for f in sorted(S):
        s = S[f]
        tot.update(s)
        rows.append(f"0x{f:08x}\t{NAMES.get(f, '?')}\t{s['jobs']}\t{s['equal_jobs']}\t{s['ink']}\t{s['ft_ink']}\t{s['differ']}\t"
                    f"{s['differ'] / max(s['ink'], 1):.3f}\t{s['e1']}\t{s['e2_3']}\t{s['e4_7']}\t{s['e8']}")
    rows.append(f"all\t\t{tot['jobs']}\t{tot['equal_jobs']}\t{tot['ink']}\t{tot['ft_ink']}\t{tot['differ']}\t"
                f"{tot['differ'] / max(tot['ink'], 1):.3f}\t{tot['e1']}\t{tot['e2_3']}\t{tot['e4_7']}\t{tot['e8']}")
    txt = "\n".join(rows) + "\n"
    print(txt, end="")
    if a.out:
        pathlib.Path(a.out).write_text(txt)


if __name__ == "__main__":
    main()
