"""reports_maps: bubble measurement on a sheet cell (rim circle from the outermost dark pixels, body circle by title.measure_body),
glyph coverage dump. Numbers only."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import fit              # noqa: E402
import title            # noqa: E402


def rim_circle(rgb, mask, box, seed=None, seed_r=None, dark_l=64, near_k=1.25):
    """The navy rim's circle inside box (x, y, w, h) of a sheet: outermost dark pixels around the dark pixels' middle."""
    x, y, w, h = box
    sub = rgb[y:y + h, x:x + w]
    L = fit.lum(sub)
    dark = mask[y:y + h, x:x + w] & (L < dark_l)
    if seed is not None:
        jj, ii = np.mgrid[0:h, 0:w]
        dark &= np.hypot(ii + 0.5 + x - seed[0], jj + 0.5 + y - seed[1]) < seed_r * near_k
    j, i = np.nonzero(dark)
    c0 = (i.mean() + 0.5, j.mean() + 0.5) if seed is None else (seed[0] - x, seed[1] - y)
    for _ in range(3):
        pts = fit.outermost(dark, c0)
        cx, cy, r, res = fit.fit_circle(pts, trim=0.8)
        c0 = (cx, cy)
    return cx + x, cy + y, r + 0.15, res


def body_circle(rgb, c):
    return title.measure_body(rgb, *c[:3])


def coverage(rgb, c, k=0.86):
    """dark glyph coverage inside k x the body radius (frame-size array)."""
    cx, cy, r = c[:3]
    x0, y0, x1, y1 = int(cx - r), int(cy - r), int(np.ceil(cx + r)), int(np.ceil(cy + r))
    jj, ii = np.mgrid[y0:y1, x0:x1]
    inside = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) < r * k
    cov, lo, hi = fit.dark_coverage(rgb[y0:y1, x0:x1], inside)
    return cov, x0, y0


def dump_cov(cov, x0, y0):
    rows = np.nonzero(cov.max(axis=1) > 0.12)[0]
    cols = np.nonzero(cov.max(axis=0) > 0.12)[0]
    j0, j1, i0, i1 = rows[0] - 1, rows[-1] + 2, cols[0] - 1, cols[-1] + 2
    out = ["     " + "".join(str((x0 + i) // 10 % 10) for i in range(i0, i1)),
           "     " + "".join(str((x0 + i) % 10) for i in range(i0, i1))]
    for j in range(j0, j1):
        out.append(f"{y0 + j:4d} " + "".join("#" if c > 0.85 else "+" if c > 0.6 else ":" if c > 0.35 else "." if c > 0.12 else " "
                                             for c in cov[j, i0:i1]))
    return "\n".join(out)


def runs(cov, x0, y0, thr=0.12, axis=0):
    """Per row (axis 0) or column (axis 1): the sub-pixel runs of the glyph, [(start, end)], where a partly covered end
    pixel contributes its coverage (start = first full-ish pixel - partial coverage of the pixel before)."""
    c = cov if axis == 0 else cov.T
    o_run, o_line = (x0, y0) if axis == 0 else (y0, x0)
    out = []
    for j in range(c.shape[0]):
        v = c[j]
        on = v > thr
        rs = []
        i = 0
        while i < len(v):
            if on[i]:
                k = i
                while k < len(v) and on[k]:
                    k += 1
                tot = float(v[i:k].sum())
                # place the run's mass: start at the first pixel's far side minus its coverage
                s = i + 1 - float(v[i])
                rs.append((round(o_run + s, 1), round(o_run + s + tot, 1)))
                i = k
            else:
                i += 1
        if rs:
            out.append((o_line + j, rs))
    return out
