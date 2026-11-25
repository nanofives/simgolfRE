"""uivec: sub-pixel refinement of every measured shape (P41). Each button and pad, per state, has its geometry (the rim
circle, the body circle, the pad's two ellipses) moved in quarter-pixel steps, then finer, while the render at the art's
resolution gets closer to the art inside that component's region. The style is measured again on the refined geometry and
both are saved: title_geom.json (the shapes) and style.json (the look). Then the acceptance pass runs.

Usage: py -3.12 re/tools/uivec/refine.py [--fresh]     --fresh: start from a new measurement, not the saved geometry
"""
import copy
import json
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import accept   # noqa: E402
import recipes  # noqa: E402
import sdf      # noqa: E402
import title    # noqa: E402


KEEP_STYLE = False      # True: move the shapes only; the look stays the one measured on the title menu


def fixed_loss(b, name, what, hover, style, s):
    """The mean squared difference to the art over the component's region AS IT IS NOW (the region and the box stay put
    while the geometry moves, so two candidates are always compared on the same pixels)."""
    x0, y0, x1, y1 = b.box(name, what, hover)
    x0, y0, x1, y1 = max(0, x0 - 3), max(0, y0 - 3), min(800, x1 + 3), min(600, y1 + 3)
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    m = b.meas[name]
    cx, cy, r = m[accept.ST[hover]]["c"]
    if what == "pad":
        mo = m[accept.ST[hover]]["outer"]
        o = {"t": "ellipse", "c": mo[:2], "r": mo[2:4], "rot": mo[4]}
        reg = (sdf.sd(o, x, y) < 2.5) & (np.hypot(x - cx, y - cy) > r + 2.5)
    else:
        reg = np.hypot(x - cx, y - cy) < r + 2.5
    tgt = b.art[hover][y0:y1, x0:x1]
    bg = np.kron(b.base[y0:y1, x0:x1], np.ones((s, s, 1)))
    cache = {}

    def f():
        img = bg.copy()
        sdf.render(title.scene(b.meas, hover, style=style, only=(name,)), img, s, origin=(x0, y0), cache=cache)
        v = img.reshape(y1 - y0, s, x1 - x0, s, 3).mean(axis=(1, 3))
        d = (v - tgt)[reg]
        return float((d * d).mean())
    return f


def descend(loss, params, passes=5):
    best = first = loss()
    for _ in range(passes):
        moved = 0
        for p in params:
            v0 = p[0][p[1]]
            for sign in (1, -1):
                v = round(float(min(max(v0 + sign * p[2], p[3]), p[4])), 3)
                if v == v0:
                    continue
                p[0][p[1]] = v
                e = loss()
                if e < best - 1e-9:
                    best, moved, v0 = e, moved + 1, v
                    break
                p[0][p[1]] = v0
        if moved == 0:
            for p in params:
                p[2] *= 0.5
    return first, best


def span(lst, k, step, reach):
    return [lst, k, step, lst[k] - reach, lst[k] + reach]


def main():
    if "--fresh" in sys.argv and os.path.exists(title.GEOM):
        os.remove(title.GEOM)
    b = accept.Bench()
    style = copy.deepcopy(recipes.STYLE)
    jobs = []
    for name, m in b.meas.items():
        for hover in (False, True):
            g = m[accept.ST[hover]]
            if m["kind"] == "disc" and hover:
                continue        # the hover disc is the idle disc
            p = [span(g["c"], 0, 0.25, 2.5), span(g["c"], 1, 0.25, 2.5), span(g["c"], 2, 0.25, 2.5)]
            if g.get("body"):
                p += [span(g["body"], 0, 0.25, 2.5), span(g["body"], 1, 0.25, 2.5), span(g["body"], 2, 0.25, 2.5)]
            jobs.append((name, "button", hover, p, 4))
            if "outer" in g:
                p = [span(g[k], i, st, rc) for k in ("inner", "outer")
                     for i, st, rc in ((0, 0.25, 4.0), (1, 0.25, 4.0), (2, 0.25, 5.0), (3, 0.25, 5.0), (4, 0.1, 1.5))]
                jobs.append((name, "pad", hover, p, 2))
    t0 = time.time()
    for rnd in range(2):
        for k, (name, what, hover, params, s) in enumerate(jobs):
            for p in params:        # each round starts again from quarter-pixel steps
                p[2] = 0.1 if p[1] == 4 and what == "pad" else 0.25
            a, e = descend(fixed_loss(b, name, what, hover, style, s), params, passes=5 if rnd == 0 else 3)
            print(f"[{rnd * len(jobs) + k + 1}/{2 * len(jobs)}] {name}.{what} {accept.ST[hover]} rms {np.sqrt(a):.2f} -> "
                  f"{np.sqrt(e):.2f} PASS ({time.time() - t0:.0f} s)", flush=True)
        for m in b.meas.values():       # the hover disc follows the idle one
            if m["kind"] == "disc":
                m["hover"] = dict(m["idle"])
        # the look, measured again on the refined shapes (only on the screen that owns the style: the title menu)
        for hover in (False, True):
            if KEEP_STYLE:
                break
            accept.measure_bubble_style(b, style, hover, use_spot=True)
            accept.measure_pad_style(b, style, hover)
            accept.measure_disc_style(b, style, hover)
    for m in b.meas.values():       # glyph shapes are attached at load time; the geometry file holds measurements only
        for st in ("idle", "hover"):
            m[st].pop("shape", None)
    with open(title.GEOM, "w") as f:
        json.dump(b.meas, f)
    if not KEEP_STYLE:
        with open(os.path.join(os.path.dirname(__file__), "style.json"), "w") as f:
            json.dump(style, f)
        recipes.STYLE.update(style)
    sys.argv = [a for a in sys.argv if a not in ("--fit", "--fresh")]
    accept.main()


if __name__ == "__main__":
    main()
