"""reports_maps: a flat box whose LIGHT is measured, not guessed (rule 6): an authored shape S, an optional navy line along its
edge, the fill painted by a bevel of S (colour by depth from the nearest edge and by which way that edge faces: the inner
shadow under a top wall, the highlight on a wall facing the light) and an optional outer band (a drop shadow, or the shaded
lip around a recess) painted the same way on S grown outward. All paints are solved by least squares on the art's pixels of
that component (common.solve_bevel_near: direction terms only near the edge, so a box's middle has no seams).

style: {"ow": line width (0 = no line), "line": rgb, "in": {"L", "stops", "coef"}, "out": {"w", "stops", "coef"} | None}
"""
import numpy as np

import common as C
import sdf

IN_STOPS = [0.0, 0.12, 0.25, 0.4, 0.6, 0.8, 1.0]
OUT_STOPS = [0.0, 0.15, 0.3, 0.5, 0.7, 0.85, 1.0]


def pixel_stops(L):
    """Stops at the pixel centres a straight edge has inside a band of L px (and 1.0): every stop has data. (A stop at 0 has
    none, half a pixel from the edge, and the solve made it up: a white rim inside the line.)"""
    n = max(int(round(L)), 1)
    return [round((k + 0.5) / L, 4) for k in range(0, n, 2) if (k + 0.5) / L < 1.0] + [1.0]   # every 2 px: no banding


def new_style(ow=0.0, L=6.0, out_w=0.0, line=(32, 42, 76)):
    return {"ow": ow, "line": list(line), "in": {"L": L, "stops": pixel_stops(L), "coef": None},
            "out": ({"w": out_w, "stops": pixel_stops(out_w), "coef": None} if out_w else None)}


def grow(S, d):
    return S if not d else {"t": "grow", "of": S, "d": d}


def layers(S, st):
    L = []
    h = st["ow"] / 2
    d = st.get("drop")
    if d:       # a soft shadow cast down-right onto whatever lies under the box (alpha, so it darkens what is there)
        L.append({"shape": {"t": "move", "of": grow(S, h + d["g"]), "d": d["d"]}, "fill": d["rgb"], "alpha": d["a"],
                  "soft": d["soft"]})
    o = st.get("out")
    if o and o["coef"] is not None:
        G = grow(S, h + o["w"])
        L.append({"shape": {"t": "sub", "a": G, "b": grow(S, h)}, "fill": {"bevel": {"L": o["w"], "stops": o["stops"],
                                                                                   "coef": o["coef"], "of": G}}})
    if st["ow"]:
        L.append({"shape": grow(S, h), "fill": st["line"]})
    F = grow(S, -h)
    L.append({"shape": F, "fill": {"bevel": {"L": st["in"]["L"], "stops": st["in"]["stops"], "coef": st["in"]["coef"]}}})
    return L


def rows_for(S, st, rgb, box, keep=None):
    """The pixels of one instance for the solve: (in rows, out rows, line pixels). box = (x0, y0, x1, y1) of the sheet;
    keep: a bool mask over the box of the pixels that belong to this component (others drawn over it are left out)."""
    x0, y0, x1, y1 = box
    jj, ii = np.mgrid[y0:y1, x0:x1]
    x, y = ii + 0.5, jj + 0.5
    art = rgb[y0:y1, x0:x1]
    h = st["ow"] / 2
    keep = np.ones(x.shape, bool) if keep is None else keep
    dF = sdf.sd(grow(S, -h), x, y)
    nx, ny = sdf._normal(dF)
    m = (dF < -0.05) & keep
    rin = ((-dF / st["in"]["L"])[m], nx[m], ny[m], art[m])
    rout = None
    o = st.get("out")
    if o:
        G = grow(S, h + o["w"])
        dG = sdf.sd(G, x, y)
        gx, gy = sdf._normal(dG)
        dS = sdf.sd(grow(S, h), x, y)
        m2 = (dG < -0.05) & (dS > 0.05) & keep
        rout = ((-dG / o["w"])[m2], gx[m2], gy[m2], art[m2])
    line = None
    if st["ow"]:
        dS = sdf.sd(S, x, y)
        m3 = (np.abs(dS) < max(h - 0.5, 0.25)) & keep
        line = art[m3]
    return rin, rout, line


def solve(insts, st, dir_in=0.6, dir_out=1.0):
    """insts: [(S, rgb, box, keep)] sharing one style; fills st's paints."""
    rin, rout, lines = [], [], []
    for S, rgb, box, keep in insts:
        a, b, c = rows_for(S, st, rgb, box, keep)
        rin.append(a)
        if b is not None:
            rout.append(b)
        if c is not None and len(c):
            lines.append(c)
    # a light ridge (lam_k 1e-6): a big flat interior must not pull the few edge pixels' direction terms toward zero
    st["in"]["coef"] = C.solve_bevel_near(rin, st["in"]["stops"], dir_until=dir_in, lam_k=1e-4)[0]
    if rout:
        st["out"]["coef"] = C.solve_bevel_near(rout, st["out"]["stops"], dir_until=dir_out, lam_k=1e-4)[0]
    if lines:
        st["line"] = [round(float(v), 1) for v in np.median(np.concatenate(lines), axis=0)]
    return st
