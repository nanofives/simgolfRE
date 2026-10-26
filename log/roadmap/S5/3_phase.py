"""S5.3: per glyph, the set of extra x offsets (26.6 sample units added to 3_fit2's predicted shift) at which the model's
level map equals GDI's exactly, and the offset with the fewest differing levels. Shows whether one constant offset (or a
different rounding of the shift) explains a font. Usage: 3_phase.py <fontref hex> [chars] [--interp 40] [--step 4]"""
import argparse
import importlib
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
G = importlib.import_module("3_gdi")
L = importlib.import_module("3_gdi_levels")
F = importlib.import_module("3_fit")
F2 = importlib.import_module("3_fit2")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("font")
    ap.add_argument("chars", nargs="?", default="".join(chr(i) for i in range(0x21, 0x7f)))
    ap.add_argument("--interp", type=int, default=40)
    ap.add_argument("--step", type=int, default=4)
    ap.add_argument("--range", type=int, default=128)
    ap.add_argument("--xfac", type=int, default=1)
    ap.add_argument("--adv", default="linear")
    a = ap.parse_args()
    face, lh = G.FONTS[int(a.font, 16)]
    g = G.Gdi()
    f = g.font(face, lh)
    pr = F.Probe()
    offs = list(range(-a.range, a.range + 1, a.step))
    hist = np.zeros(len(offs), int)
    for ch in a.chars:
        ref = L.levels(g, f, ch.encode("latin-1"))
        bw = sum(g.abc(f, ord(ch)))
        ds = []
        for o in offs:
            m = F2.Model(pr, a.interp, a.xfac, F.NO_BITMAP, a.adv, "none", o)
            ds.append(int((F.box_levels(m.samples(F.FILES[face], -lh, ord(ch), bw)) != ref).sum()))
        ds = np.array(ds)
        ok = [offs[i] for i in np.nonzero(ds == 0)[0]]
        hist += ds == 0
        m = F2.Model(pr, a.interp, a.xfac, F.NO_BITMAP, a.adv, "none", 0)
        gid = pr.d.pr_char(pr.face(F.FILES[face], f"m{a.interp}"), ord(ch))
        nat = m.natural(pr.face(F.FILES[face], f"m{a.interp}"), gid, -lh)
        pred = (bw * 6 * 64 - nat) / 2
        print(f"{ch!r} bw {bw} nat {nat / 64:.3f} pred {pred:.1f}  equal at {ok[:1]}..{ok[-1:]} ({len(ok)})"
              f"  best {offs[int(ds.argmin())]} ({ds.min()})")
    print("offsets equal for most glyphs:", sorted(zip(hist, offs))[-8:])


if __name__ == "__main__":
    main()
