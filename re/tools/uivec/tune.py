"""uivec: the bubble's rim width and cast shadow, chosen against the art (P41), then the acceptance pass.
- rim width: each candidate width gets the body's shading measured for it; the one whose buttons are closest to the art wins.
- cast shadow: its offset, softness and strength by coordinate descent on the pads AND the ring just outside each rim
  (the first fit only looked at the pads, away from the rim, and left the shadow too weak next to the bubble).
Usage: py -3.12 re/tools/uivec/tune.py
"""
import copy
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import accept   # noqa: E402
import recipes  # noqa: E402


def main():
    b = accept.Bench()
    style = copy.deepcopy(recipes.STYLE)
    B = style["bubble"]
    bubbles = [n for n, m in b.meas.items() if m["kind"] == "bubble"]

    def measure_all():
        for hover in (False, True):
            accept.measure_bubble_style(b, style, hover, use_spot=True)
            accept.measure_pad_style(b, style, hover)
            accept.measure_disc_style(b, style, hover)

    def button_mse(inner_only):
        tot, cnt = 0.0, 0
        for n in bubbles:
            for hv in (False, True):
                cx, cy, r = b.meas[n][accept.ST[hv]]["c"]
                x, y = b.grid(n, "button", hv)
                rr = np.hypot(x - cx, y - cy)
                reg = (rr < r + 0.5) if inner_only else ((rr > r + 0.5) & (rr < r + 5))
                d = (b.render(n, "button", hv, style, 4) - b.target(n, "button", hv))[reg]
                tot += float((d * d).sum())
                cnt += d.size
        return tot, cnt

    best = None
    widths = [1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 3.0]
    for k, w in enumerate(widths):
        B["rimw"] = w
        measure_all()
        t, c = button_mse(True)
        e = np.sqrt(t / c)
        print(f"[{k + 1}/{len(widths)}] rim width {w:.2f} px: rms {e:.3f} PASS", flush=True)
        if best is None or e < best[0]:
            best = (e, w)
    B["rimw"] = best[1]
    measure_all()
    print(f"rim width {best[1]} px", flush=True)

    def shadow_loss():
        t1, c1 = button_mse(False)
        t2, c2 = 0.0, 0
        for n in bubbles:
            for hv in (False, True):
                reg = b.region(n, "pad", hv)
                d = (b.render(n, "pad", hv, style, 2) - b.target(n, "pad", hv))[reg]
                t2 += float((d * d).sum())
                c2 += d.size
        return t1 / c1 + t2 / c2

    sh = B["sh"]
    accept.descend(shadow_loss, [[sh, 0, 0.04, -0.6, 0.6], [sh, 1, 0.04, -0.6, 0.8], [sh, 2, 0.04, 0.02, 0.8],
                                 [sh, 3, 0.08, 0.0, 1.0]], passes=6, label="cast shadow")
    B["sh"] = [round(v, 3) for v in sh]
    print("cast shadow", B["sh"], flush=True)
    with open(os.path.join(os.path.dirname(__file__), "style.json"), "w") as f:
        json.dump(style, f)
    recipes.STYLE.update(style)
    accept.main()


if __name__ == "__main__":
    main()
