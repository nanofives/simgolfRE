"""Fit the medal bubble body as a radial gradient (light centre searched on a grid, colours by least squares)."""
import numpy as np
import common as C
import el_medals as M
import glyphmap as G
import fit

T = [0.0, 0.25, 0.5, 0.75, 1.0]


def hat(t):
    t = np.clip(t, 0, 1)
    W = np.zeros((len(t), len(T)))
    for k in range(len(T) - 1):
        a, b = T[k], T[k + 1]
        inside = (t >= a) & (t <= b)
        f = (t - a) / (b - a)
        W[inside, k] += 1 - f[inside]
        W[inside, k + 1] += f[inside]
    return W


def fit_radial(art, mask, R):
    h, w = mask.shape
    jj, ii = np.mgrid[0:h, 0:w]
    x, y = ii[mask] + 0.5, jj[mask] + 0.5
    best = None
    for cx in np.arange(4, 20, 0.5):
        for cy in np.arange(4, 20, 0.5):
            t = np.hypot(x - cx, y - cy) / R
            W = hat(t)
            cols = np.linalg.lstsq(W, art[mask], rcond=None)[0]
            res = np.abs(W @ cols - art[mask]).mean()
            if best is None or res < best[0]:
                best = (res, cx, cy, cols)
    return best


if __name__ == "__main__":
    for k in ("cash", "skill"):
        for st in (0, 1):
            x, y, w, h = M.CELLS[k][st]
            art, m = C.crop(C.CI, x, y, w, h)
            cx, cy, r = M.outer_circle(art, m)
            L = fit.lum(art)
            d = G.box(L, 3) - L
            jj, ii = np.mgrid[0:h, 0:w]
            dist = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy)
            far = d < 6
            # exclude glyph neighbourhood: dilate the dark map by 2
            g = d > 10
            gd = g.copy()
            for _ in range(2 if st else 3):
                gd = gd | np.roll(gd, 1, 0) | np.roll(gd, -1, 0) | np.roll(gd, 1, 1) | np.roll(gd, -1, 1)
            mask = m & (dist < r - 2.6) & ~gd
            R = r + 4
            res, lx, ly, cols = fit_radial(art, mask, R)
            print(k, "ab"[st], f"circle c=({cx:.2f},{cy:.2f}) r={r:.2f} light=({lx},{ly}) R={R:.1f} mad={res:.1f}")
            print("   stops", [[t, [int(v) for v in c]] for t, c in zip(T, cols)])
            # rim colour by angle
            rim = m & (dist > r - 2.2) & (dist < r - 0.5)
            ang = np.degrees(np.arctan2(jj + 0.5 - cy, ii + 0.5 - cx))
            for a0 in range(-180, 180, 45):
                s = rim & (ang >= a0) & (ang < a0 + 45)
                print(f"   rim {a0:4d}: {art[s].mean(0).astype(int) if s.any() else None}")
