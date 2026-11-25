"""Compare authored digits against the art coverage (lit cells); optional tune (glyphs.fit_shape)."""
import copy, json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C, bub, digits as D
import glyphs

rgb, m = C.sheet('buy_land_buttons.pcx')


def art_cov(n, state='lit'):
    i = n - 1
    box = (0x3c * i + 1, 1 if state == 'lit' else 0x40, 0x3b, 0x3d)
    c = bub.rim_circle(rgb, m, box)
    b = bub.body_circle(rgb, c)
    cx, cy, r = b
    x0, y0, x1, y1 = int(cx - r), int(cy - r), int(np.ceil(cx + r)), int(np.ceil(cy + r))
    jj, ii = np.mgrid[y0:y1, x0:x1]
    inside = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) < r * 0.9
    import fit
    cov, lo, hi = fit.dark_coverage(rgb[y0:y1, x0:x1], inside)
    return cov, inside, (x0, y0, x1, y1), b, c


def iou(a, b):
    return float(np.minimum(a, b).sum() / max(np.maximum(a, b).sum(), 1e-9))


if __name__ == '__main__':
    tune = '--fit' in sys.argv
    tiles = []
    res = {}
    for n in range(1, 10):
        cov, inside, box, b, c = art_cov(n)
        parts = D.authored()[n]
        sh = D.U(*[p for _, p in parts])
        sc = glyphs.shape_coverage(sh, box, inside)
        i0 = iou(cov, sc)
        if tune:
            f0, f1, k = glyphs.fit_shape(sh, cov, inside, box, passes=6, reach=1.0)
            sc = glyphs.shape_coverage(sh, box, inside)
            res[n] = sh
        i1 = iou(cov, sc)
        print(f"[{n}/9] digit {n} iou {i0:.3f} -> {i1:.3f}", flush=True)
        z = 10
        a = np.kron(cov, np.ones((z, z))) * 255
        bb = np.kron(sc, np.ones((z, z))) * 255
        dd = np.stack([a, bb, np.zeros_like(a)], -1)
        tiles.append(np.concatenate([np.stack([a] * 3, -1), np.full((a.shape[0], 4, 3), 128.), np.stack([bb] * 3, -1),
                                     np.full((a.shape[0], 4, 3), 128.), dd], 1))
    H = max(t.shape[0] for t in tiles)
    C.save_png(C.stack(tiles[:5]), os.path.join(C.OUT, 'read', 'digits_a.png'))
    C.save_png(C.stack(tiles[5:]), os.path.join(C.OUT, 'read', 'digits_b.png'))
    if tune:
        json.dump(res, open(os.path.join(C.OUT, 'read', 'digits_fit.json'), 'w'))
