"""reports_maps: the numeric fit that only TUNES a read scene (P41 rule 5). A parameter is [container, key, step, lo, hi]; each
moves at most `reach` from its read value. Loss: mean squared difference between the scene rendered at s x and averaged down,
and the art, over a mask."""
import time

import numpy as np

import common as C
import sdf


def render_low(scene, base, origin, s=4):
    h, w = base.shape[:2]
    img = np.kron(base, np.ones((s, s, 1)))
    sdf.render(scene, img, s, origin=origin)
    return img.reshape(h, s, w, s, 3).mean(axis=(1, 3))


def make_loss(scene, base, art, origin, mask=None, s=4):
    def loss():
        low = render_low(scene, base, origin, s)
        d = (low - art) ** 2
        return float(d[mask].mean() if mask is not None else d.mean())
    return loss


def descend(loss, params, passes=5, reach=None, verbose=False, label=""):
    start = [p[0][p[1]] for p in params]
    best = first = loss()
    t0 = time.time()
    for ps in range(passes):
        moved = 0
        for p, v0s in zip(params, start):
            cont, key, step = p[0], p[1], p[2]
            lo = p[3] if len(p) > 3 else None
            hi = p[4] if len(p) > 4 else None
            r = reach if reach is not None else (p[5] if len(p) > 5 else 8 * abs(step))
            v0 = cont[key]
            for sign in (1, -1):
                v = round(v0 + sign * step, 4)
                if abs(v - v0s) > r + 1e-9 or (lo is not None and v < lo) or (hi is not None and v > hi):
                    continue
                cont[key] = v
                e = loss()
                if e < best - 1e-9:
                    best, moved = e, moved + 1
                    break
                cont[key] = v0
        if verbose:
            print(f"  {label} pass {ps + 1}: {best:.2f} (from {first:.2f}), {moved} moved, {time.time() - t0:.0f}s", flush=True)
        if moved == 0:
            for p in params:
                p[2] *= 0.5
    return first, best
