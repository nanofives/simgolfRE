"""S5.3: the 'mixed' outline: x coordinates from the unhinted outline, y coordinates from the hinted one (interpreter --yinterp),
placed with an x offset, x scaled by 6, y by V, monochrome scan, 6 x V box, level = round(count / V). Per glyph, sweeps the
offset (1/64 sample steps of --step) and prints the best offsets with the glyph's metrics, so a placement rule can be read
off. Usage: 3_mix.py <fontref hex> [chars] [--v 6] [--yinterp 35] [--step 8] [--range 512]"""
import argparse
import ctypes as c
import importlib
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
G = importlib.import_module("3_gdi")
L = importlib.import_module("3_gdi_levels")
F = importlib.import_module("3_fit")

OX, OY, W, H, S = L.OX, L.OY, L.W, L.H, 6
MAXP = 4096


class Mix:
    def __init__(self, pr, yinterp=35, v=6):
        self.pr, self.yinterp, self.v = pr, yinterp, v
        d = pr.d
        d.pr_render2.argtypes = [c.c_void_p, c.c_long, c.c_long, c.c_long, c.c_long, c.c_void_p, c.c_int, c.c_int]
        d.pr_points.argtypes = [c.c_void_p, c.c_void_p, c.c_void_p, c.c_void_p, c.c_void_p, c.c_int]
        d.pr_set_points.argtypes = [c.c_void_p, c.c_void_p]
        self.cache = {}

    def outline(self, path, ppem, ch):
        """(face, xs unhinted, ys hinted, metrics) with the hinted glyph loaded in the slot and its points replaced"""
        d = self.pr.d
        f = self.pr.face(path, f"mix{self.yinterp}")
        d.pr_interp(self.yinterp)
        d.pr_size(f, ppem, ppem)
        gid = d.pr_char(f, ch)
        xy = (c.c_long * (2 * MAXP))()
        adv, npts = c.c_int(), c.c_int()
        d.pr_load(f, gid, F.NO_BITMAP | F.NO_HINTING, c.byref(adv), c.byref(npts), None)
        nat_adv = adv.value
        n = d.pr_points(f, xy, None, None, None, MAXP)
        ux = np.array(xy[0:2 * n:2])
        d.pr_load(f, gid, F.NO_BITMAP, c.byref(adv), c.byref(npts), None)
        n2 = d.pr_points(f, xy, None, None, None, MAXP)
        if n2 != n:
            raise RuntimeError("point counts differ")
        hx, hy = np.array(xy[0:2 * n:2]), np.array(xy[1:2 * n:2])
        return f, ux, hx, hy, nat_adv, adv.value

    def render(self, f, xs, ys, dx):
        d = self.pr.d
        n = len(xs)
        buf = (c.c_long * (2 * n))()
        buf[0::2] = [int(v) for v in xs]
        buf[1::2] = [int(v) for v in ys]
        d.pr_set_points(f, buf)
        V = self.v
        smp = np.zeros((H * V, W * S), np.uint8)
        if n:
            e = d.pr_render2(f, S << 16, V << 16, OX * S * 64 + dx, (H - OY) * V * 64,
                             smp.ctypes.data_as(c.c_void_p), W * S, H * V)
            if e:
                raise RuntimeError(f"render {e}")
        cnt = F.box_levels(smp).reshape(H, V, W, 3).sum(1)
        return (2 * cnt + V) // (2 * V)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("font")
    ap.add_argument("chars", nargs="?", default="".join(chr(i) for i in range(0x21, 0x7f)))
    ap.add_argument("--v", type=int, default=6)
    ap.add_argument("--yinterp", type=int, default=35)
    ap.add_argument("--step", type=int, default=8)
    ap.add_argument("--range", type=int, default=640)
    ap.add_argument("--hx", action="store_true", help="use the hinted x as well (control)")
    a = ap.parse_args()
    face, lh = G.FONTS[int(a.font, 16)]
    g = G.Gdi()
    ft = g.font(face, lh)
    m = Mix(F.Probe(), a.yinterp, a.v)
    tot_best = 0
    n_eq = 0
    for ch in a.chars:
        ref = L.levels(g, ft, ch.encode("latin-1"))
        abc = g.abc(ft, ord(ch))
        f, ux, hx, hy, nat, hadv = m.outline(F.FILES[face], -lh, ord(ch))
        xs = hx if a.hx else ux
        best = None
        res = []
        for dx in range(-a.range, a.range + 1, a.step):
            dd = int((m.render(f, xs, hy, dx) != ref).sum())
            res.append((dd, dx))
            if best is None or dd < best[0]:
                best = (dd, dx)
        eqs = [dx for dd, dx in res if dd == best[0]]
        tot_best += best[0]
        n_eq += best[0] == 0
        uxmin = ux.min() if len(ux) else 0
        hxmin = hx.min() if len(hx) else 0
        print(f"{ch!r} abc {abc} nat {nat / 64:.3f} hadv {hadv / 64:.0f} uxmin {uxmin / 64:.3f} hxmin {hxmin / 64:.3f}"
              f"  best diff {best[0]} at dx {eqs[0] / 64:.3f}..{eqs[-1] / 64:.3f} samples"
              f"  (half-diff rule {(sum(abc) * 64 - nat) * 3 / 64:.3f})", flush=True)
    print(f"TOTAL best-offset differing levels {tot_best}, glyphs exact at their best offset {n_eq}/{len(a.chars)}")


def correlate():
    """best offset per glyph against displacements the BW (v35, 1x) hinting gives the outline's points, in samples"""
    ap = argparse.ArgumentParser()
    ap.add_argument("font")
    ap.add_argument("--v", type=int, default=6)
    ap.add_argument("--step", type=int, default=16)
    ap.add_argument("--range", type=int, default=640)
    ap.add_argument("--correlate", action="store_true")
    a = ap.parse_args()
    face, lh = G.FONTS[int(a.font, 16)]
    g = G.Gdi()
    ft = g.font(face, lh)
    m = Mix(F.Probe(), 35, a.v)
    rows = []
    for ch in (chr(i) for i in range(0x21, 0x7f)):
        ref = L.levels(g, ft, ch.encode("latin-1"))
        f, ux, hx, hy, nat, hadv = m.outline(F.FILES[face], -lh, ord(ch))
        if not len(ux):
            continue
        res = [(int((m.render(f, ux, hy, dx) != ref).sum()), dx) for dx in range(-a.range, a.range + 1, a.step)]
        best = min(res)[1] / 64
        bw = sum(g.abc(ft, ord(ch)))
        cand = {"min": 6 * (hx.min() - ux.min()) / 64, "max": 6 * (hx.max() - ux.max()) / 64,
                "mean": 6 * (hx - ux).mean() / 64, "ctr": 3 * ((hx.min() + hx.max()) - (ux.min() + ux.max())) / 64,
                "half": 3 * (bw * 64 - nat) / 64, "p0": 6 * (hx[0] - ux[0]) / 64, "ux6": 6 * ux.min() / 64, "hx6": 6 * hx.min() / 64, "uxmax6": 6 * ux.max() / 64, "hxmax6": 6 * hx.max() / 64, "bw6": 6 * bw}
        rows.append((ch, best, cand))
    best = np.array([r[1] for r in rows])
    print(f"{len(rows)} glyphs, best offset sd {best.std():.2f} samples")
    for k in rows[0][2]:
        c = np.array([r[2][k] for r in rows])
        print(f"  {k:5s} corr {np.corrcoef(best, c)[0, 1]:+.3f}  residual sd {np.std(best - c):.2f}  mean diff {np.mean(best - c):+.2f}")
    for ch, b, cand in rows:
        print(f"  {ch!r} best {b:+.2f} " + " ".join(f"{k} {v:+.2f}" for k, v in cand.items()))


def rules():
    """total differing levels per font for placement rules of the mixed outline (x unhinted, y v35-hinted, V subrows)"""
    ap = argparse.ArgumentParser()
    ap.add_argument("fonts")
    ap.add_argument("--v", type=int, default=6)
    ap.add_argument("--rules", action="store_true")
    ap.add_argument("--yinterp", type=int, default=35)
    a = ap.parse_args()
    g = G.Gdi()
    pr = F.Probe()
    m = Mix(pr, a.yinterp, a.v)
    mb = Mix(pr, 35, a.v)   # the BW (v35) hinting, for the x displacement of its leftmost point
    for fk in a.fonts.split(","):
        face, lh = G.FONTS[int(fk, 16)]
        ft = g.font(face, lh)
        tot = {}
        for ch in (chr(i) for i in range(0x21, 0x7f)):
            ref = L.levels(g, ft, ch.encode("latin-1"))
            _f, _ux, hx, _hy, _n, _h = mb.outline(F.FILES[face], -lh, ord(ch))
            f, ux, hx_y, hy, nat, hadv = m.outline(F.FILES[face], -lh, ord(ch))
            if not len(ux):
                continue
            bw = sum(g.abc(ft, ord(ch)))
            cands = {"zero": 0, "half": int(np.floor(3 * (bw * 64 - nat) + 0.5)), "min": int(6 * (hx.min() - ux.min())),
                     "mean": int(np.floor(6 * (hx - ux).mean() + 0.5)), "hx": None}
            for k, dx in cands.items():
                lv = m.render(f, hx, hy, 0) if dx is None else m.render(f, ux, hy, dx)
                tot[k] = tot.get(k, 0) + int((lv != ref).sum())
            # the drawn outline's own (yinterp-hinted) x with the min shift: what fttext.cpp draws
            lv = m.render(f, hx_y, hy, cands["min"])
            tot["min_ownx"] = tot.get("min_ownx", 0) + int((lv != ref).sum())
            lv = m.render(f, hx_y, hy, 0)
            tot["zero_ownx"] = tot.get("zero_ownx", 0) + int((lv != ref).sum())
            lv = m.render(f, hx_y, hy, cands["half"])
            tot["half_ownx"] = tot.get("half_ownx", 0) + int((lv != ref).sum())
        print(f"0x{int(fk, 16):x} {face} {-lh}: " + "  ".join(f"{k} {v}" for k, v in tot.items()), flush=True)


if __name__ == "__main__" and "--rules" in sys.argv:
    rules()
    sys.exit(0)

if __name__ == "__main__" and "--correlate" in sys.argv:
    correlate()
    sys.exit(0)


if __name__ == "__main__":
    main()
