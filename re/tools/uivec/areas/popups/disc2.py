"""P41 popups fix round: the info buttons' disc body as lit layers (Mariano: "lightning is not baked", "shading").

The first round painted the body with one fitted bevel paint (colour by depth and direction), which read as a colour that
changes across a flat disc. Here the body is a raised disc lit from the upper left, built from layers:
  1. base     the body circle in its shadow-side colour (flat);
  2. light    the same circle in the lit colour, faded out from a light spot on the upper left (falloff c, r, pow);
  3. edge     a 1 px line just inside the rim, light on the upper left and dark on the lower right (sdf edge_light);
  4. bounce   a thin band inside the rim on the lower left, lighter (reflected light: the art's 190-193 row against 174).
Geometry is relative to the disc (centre, body radius rb), so one STYLE per state serves all ten buttons; it is fitted on the
body pixels of all ten at once (glyph pixels masked out), so the same component is lit the same everywhere.

py -3.12 disc2.py        -> fits, prints the error per state, writes disc2_style.json next to this file
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w2               # noqa: E402,F401
import common as c      # noqa: E402
import infobuttons as ib  # noqa: E402
import glyphs_ib        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE_PATH = os.path.join(HERE, "disc2_style.json")
RIM_W = 1.9


def layers(cx, cy, R, st):
    """The body layers of one disc (rim outer circle cx, cy, R) in style st."""
    rb = R - RIM_W
    body = c.circ(cx, cy, rb)
    g = st["geom"]
    L = [{"shape": body, "fill": st["base"]},
         {"shape": body, "fill": st["light"],
          "falloff": {"c": [cx + g["lx"] * rb, cy + g["ly"] * rb], "r": g["lr"] * rb, "pow": g["lp"]}}]
    bounce = {"t": "sub", "a": body, "b": c.circ(cx + g["bs"] * rb * 0.7071, cy - g["bs"] * rb * 0.7071, rb)}
    L.append({"shape": bounce, "fill": st["bounce"], "alpha": st["bounce_a"], "soft": g["bsoft"], "clip": body})
    L.append({"shape": body, "fill": st["light"], "alpha": 0.0,
              "fx": [{"t": "edge_light", "w": g["ew"], "pow": 1.0, "dir": [-0.7071, -0.7071],
                      "light": st["edge_l"], "dark": st["edge_d"]}]})
    return L


def _samples(state_col):
    """Body pixels of every button in one state: list of (x, y, rgb, (cx, cy, R)) flattened."""
    X, Y, C, D = [], [], [], []
    for r, k in enumerate(ib.KEYS):
        box = ib.cell(r, state_col)
        disc = ib.measure_disc(r, state_col)
        d0 = ib.measure_disc(r, 0)
        a = c.art(ib.SHEET, box)
        jj, ii = np.mgrid[0:ib.H, 0:ib.W]
        x, y = ii + 0.5 + box[0], jj + 0.5 + box[1]
        gm = np.zeros((ib.H, ib.W), bool)
        for S, _, _ in glyphs_ib.GLYPHS[k]():
            S2 = ib.moved(S, disc[0] - d0[0], 50 * r + disc[1] - d0[1])
            gm |= c.sdf.sd(S2, x, y) < 1.6
        if k == "globe":
            gm |= np.hypot(x - disc[0], y - disc[1]) < 12.4
        cx, cy, R = disc
        keep = (np.hypot(x - cx, y - cy) < R - RIM_W - 0.7) & ~gm
        X.append(x[keep]); Y.append(y[keep]); C.append(a[keep]); D.append(np.tile([cx, cy, R], (keep.sum(), 1)))
    return np.concatenate(X), np.concatenate(Y), np.concatenate(C), np.concatenate(D)


def _weights(x, y, D, g):
    cx, cy, R = D[:, 0], D[:, 1], D[:, 2]
    rb = R - RIM_W
    rho = np.hypot(x - cx, y - cy)
    f = np.clip(1 - np.hypot(x - (cx + g["lx"] * rb), y - (cy + g["ly"] * rb)) / (g["lr"] * rb), 0, 1) ** g["lp"]
    # the bounce band: inside the body, outside the body moved up-right by bs * rb
    s = g["bs"] * rb * 0.7071
    dsub = np.maximum(rho - rb, -(np.hypot(x - (cx + s), y - (cy - s)) - rb))
    u = np.clip(0.5 - dsub / (2 * g["bsoft"]), 0, 1)
    b = u * u * (3 - 2 * u)
    # the edge line: 1 within ew of the body edge, times the light direction (+ light, - dark)
    e = np.clip(1 + (rho - rb) / g["ew"], 0, 1)
    lit = ((x - cx) * -0.7071 + (y - cy) * -0.7071) / np.maximum(rho, 1e-9)
    return f, b, e * np.clip(lit, 0, 1), e * np.clip(-lit, 0, 1)


def fit_state(col):
    x, y, C, D = _samples(col)
    best = None
    for lx in (-0.7, -0.55, -0.4, -0.25):
        for ly in (-0.7, -0.55, -0.4, -0.25):
            for lr in (1.2, 1.5, 1.8, 2.2):
                for lp in (1.0, 1.5, 2.0):
                    for bs in (0.12, 0.2):
                        g = {"lx": lx, "ly": ly, "lr": lr, "lp": lp, "bs": bs, "bsoft": 0.6, "ew": 1.2}
                        f, b, el, ed = _weights(x, y, D, g)
                        # colour = base + (light - base) f, then bounce mixed in, then + edge light / - edge dark:
                        # linear in (base, dl = light - base, db, el, ed) per channel when the bounce term is taken additively
                        A = np.c_[np.ones_like(f), f, b, el, -ed]
                        sol, *_ = np.linalg.lstsq(A, C, rcond=None)
                        err = np.abs(A @ sol - C).mean()
                        if best is None or err < best[0]:
                            best = (err, g, sol)
    err, g, sol = best
    base, dl, db, el, ed = sol
    light = base + dl
    # the bounce layer is drawn over base + light falloff: colour that gives the same addition where its alpha is 1
    st = {"geom": g, "base": [round(float(v), 1) for v in base], "light": [round(float(v), 1) for v in np.clip(light, 0, 255)],
          "bounce": [round(float(v), 1) for v in np.clip(base + 0.5 * dl + db, 0, 255)], "bounce_a": 1.0,
          "edge_l": [round(float(v), 1) for v in np.clip(el, 0, 255)], "edge_d": [round(float(v), 1) for v in np.clip(ed, 0, 255)],
          "fit_err": round(float(err), 2)}
    return st


if __name__ == "__main__":
    out = {}
    for col, name in enumerate(ib.STATES):
        out[name] = fit_state(col)
        print(name, json.dumps(out[name]))
    with open(STYLE_PATH, "w") as f:
        json.dump(out, f, indent=1)
