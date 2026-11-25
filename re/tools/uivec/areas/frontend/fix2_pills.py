"""P41 fix round: the editor's option pills (CGButtons.pcx), Mariano: "lightning of the button needs to be baked, no changing
colors, actual lightning should be rendered, shape not quite there".

Read again at 7x (log/roadmap/P41/work2/frontend/_view/z_pilltall*.png) and as colour columns:
- the shape is a long rectangle whose ends are half ellipses about 4-5 px deep and the full height tall (the first round drew a
  rounded rectangle with quarter-height corners, so the ends looked boxy);
- the outline is 1 px and lit: on the idle tall pill it is a grey-lilac on top and toward the upper left and navy at the
  bottom and on the right; on the others it is navy all round. Drawn as a linear paint fitted to the line's own pixels;
- the fill changes down the rows (a light band along the top fading into the flat tan or yellow below) and is lighter toward
  the left end. Drawn as one stop per row read down the middle column (no normals, so no mitre seams, which is where the
  first round's "changing colors" came from) and a highlight fading from the left end (alpha and reach fitted);
- the shadow under it is a soft dark copy of the shape moved down (offset, softness and strength fitted on the art);
- the wires are 2 px navy rods; the lilac at the cells' right is the backdrop showing through the cell, not part of the pill
  (the first round drew it as a "step").
"""
import numpy as np

import common as c
import fix2 as F
import parts
from common import fit, sdf

CB = "Interface/CGButtons.pcx"


def measure(art, opaque, rect):
    x, y, w, h = rect
    sub = art[y:y + h, x:x + w]
    op = opaque[y:y + h, x:x + w]
    mx = w // 2
    L = fit.lum(sub[:, mx])
    D = lambda p, q: float(np.abs(p - q).max())
    k = 0
    while k < h and not (op[k, mx] and L[k] < 165):
        k += 1
    y0 = k                                   # first line row
    k = y0 + 1
    while k < h and L[k] > 165:              # the fill
        k += 1
    while k < h and L[k] < 95:               # the bottom line
        k += 1
    y1 = k
    cy = (y0 + y1) / 2
    jm = int(cy)
    rowv = sub[jm]
    Lr = fit.lum(rowv)
    i = mx
    while i > 0 and Lr[i] > 165:
        i -= 1
    bgl = rowv[0]
    while i > 0 and Lr[i - 1] < 165 and op[jm, i - 1] and D(rowv[i - 1], bgl) >= 30:
        i -= 1
    x0 = i
    i = mx
    while i < w - 1 and Lr[i] > 165:
        i += 1
    while i < w - 1 and Lr[i] > 75:
        i += 1
    while i < w - 1 and Lr[i + 1] < 75 and op[jm, i + 1]:
        i += 1
    x1 = i + 1
    # the end's depth: the leftmost line pixel per row against a half ellipse, rx searched
    pts = []
    for j in range(y0, y1):
        r_ = sub[j]
        for i in range(0, mx):
            if op[j, i] and D(r_[i], r_[0] if op[j, 0] else r_[i]) >= 30 and i <= x0 + 10:
                pts.append((i, j + 0.5))
                break
    ry = (y1 - y0) / 2
    best = None
    for rx in np.arange(2.0, 9.01, 0.25):
        err = 0.0
        for i, yy in pts:
            v = min(1.0, abs(yy - cy) / ry)
            xe = x0 + rx * (1 - np.sqrt(max(0.0, 1 - v * v)))
            err += min((xe - (i + 0.5)) ** 2, 4.0)
        if best is None or err < best[0]:
            best = (err, rx)
    return x + x0, y + y0, x + x1, y + y1, float(best[1])


def wires(art, rect, x0, y0, x1, y1):
    """Navy rods leaving the pill straight down (below the bottom line to the cell's end) or up (above the top line)."""
    x, y, w, h = rect
    L = fit.lum(art)
    out = []
    for (ya, yb) in ((y1 + 1, y + h), (y, y0)):
        if yb - ya < 1:
            continue
        cols = [i for i in range(int(x0) + 6, int(x1) - 6) if (L[ya:min(yb, ya + 3), i] < 66).mean() > 0.6]
        runs = []
        for i in cols:
            if runs and i == runs[-1][1]:
                runs[-1][1] = i + 1
            else:
                runs.append([i, i + 1])
        for a, b in runs:
            if b - a > 3:
                continue
            rgb = [round(float(v), 1) for v in np.median(art[ya:yb, a:b].reshape(-1, 3), axis=0)]
            out.append((a, b, ya, yb, rgb))
    return out


# the 2 px navy rods read off the dumps (cell origin: [(left column, direction)]); the tail idle cell has none (the dark
# swoosh under it is the backdrop's curve)
RODS = {(0, 50): [(24, "down")], (150, 50): [(174, "up"), (174, "down")], (150, 250): [(174, "up")],
        (0, 300): [(87, "up"), (87, "down")], (150, 300): [(237, "up"), (237, "down")],
        (0, 400): [(87, "up"), (87, "down")], (150, 400): [(237, "up"), (237, "down")]}


def pill(art, opaque, rect, name, rows=None):
    """rows (top line row, bottom line row) read off the art overrides the measured top and bottom (on the backdrop the
    shadow under a pill runs into the next pill's dark line, and the bottom scan followed it)."""
    x, y, w, h = rect
    x0, y0, x1, y1, rx = measure(art, opaque, rect)
    if rows is not None:
        y0, y1 = rows[0], rows[1] + 1
    outer = F.capsule(x0, y0, x1, y1, rx)
    inner = F.capsule(x0 + 1.0, y0 + 1.0, x1 - 1.0, y1 - 1.0, max(rx - 0.6, 1.0))
    X, Y = parts.grid(x, y, x + w, y + h)
    sub = art[y:y + h, x:x + w]
    # the line: a linear paint over its own pixels
    lm = (sdf.sd(outer, X, Y) < -0.25) & (sdf.sd(inner, X, Y) > 0.25)
    line_paint = parts.linear_fit(sub, lm, x, y)
    # the fill: one stop per row down the middle column, then the lit end's highlight
    mxc = int((x0 + x1) / 2)
    fill = F.profile_stops(art, mxc, y0 + 1, y1 - 1, n=max(2, int(y1 - y0 - 3)))
    im = sdf.sd(inner, X, Y) < -0.6
    light = [round(float(v), 1) for v in np.percentile(sub[im], 99, axis=0)]
    cy = (y0 + y1) / 2
    hl = F.fit_falloff(art, inner, fill, light, (x, y, x + w, y + h),
                       centres=[(x0 + dx, cy + dy) for dx in (1.0, 3.0, 5.0, 8.0) for dy in (-4.0, -2.0, 0.0, 2.0)],
                       radii=(8.0, 12.0, 16.0, 22.0, 30.0))
    # the shadow: fitted over the art with the pill and the band under it replaced, column by column, by the cell's last
    # row (the band's own background; an onion-peel fill smeared the lilac corner across the cream)
    ws = [(a, a + 2, y1 - 1 if d == "down" else y, y + h if d == "down" else y0 + 1, F.NAVY) for a, d in RODS.get(rect[:2], ())]
    bg = art.copy()
    xa, xb = max(x, int(x0) - 3), min(x + w, int(x1) + 5)
    yb_ = y + h - 1
    for i in range(xa, xb):
        bg[int(y0):yb_, i] = art[yb_, i]
    sh = F.fit_shadow(art, outer, bg, (x, y, x + w, y + h), rgb=(45, 45, 70))
    L = []
    if sh:
        L.append(sh)
    for a, b, ya, yb, rgb in ws:
        rod = F.box(a, ya, b, yb)
        L.append({"shape": F.mv(rod, 1.5, 0.5), "fill": [45, 45, 70], "soft": 1.2, "alpha": 0.45})
        L.append({"shape": rod, "fill": rgb})
    L.append({"shape": outer, "fill": line_paint})
    L.append({"shape": inner, "fill": fill})
    if hl:
        L.append({"shape": inner, "fill": light, **hl})
    item = {"box": [x, y, x + w, y + h], "layers": L}
    info = {"outer": [x0, y0, x1, y1], "end_rx": rx, "shadow": sh and {k: sh[k] for k in ("soft", "alpha")},
            "highlight": hl, "wires": [[a, b, ya, yb] for a, b, ya, yb, _ in ws]}
    return {"name": name, "art": art, "bg": bg, "scene": [item], "rect": rect}, info


def main():
    cg, op = c.sheet(CB)
    note = F.note("editor-pill")
    sts, infos = [], []
    for nm, rect in (("idle", (0, 50, 112, 28)), ("selected", (150, 50, 112, 28)), ("tail idle", (0, 250, 112, 28)),
                     ("tail selected", (150, 250, 112, 28))):
        st, info = pill(cg, op, rect, nm)
        sts.append(st)
        infos.append(info)
        print(nm, info)
    how = ("a pill whose ends are half ellipses (end depth fitted per state: "
           + ", ".join(f"{i['end_rx']:.2f}" for i in infos) + " px) in a 1 px line (a linear paint fitted to the line's "
           "pixels), the fill one stop per row read down its middle with a highlight fading from the left end, a fitted soft "
           "shadow below; the tail cells add the 2 px navy rod")
    F.deliver("editor-pill", sts, how, "lighting baked as row stops + an end highlight + a fitted cast shadow (no normal-based "
              "paint, so no colour seams); ends redrawn as half ellipses instead of rounded corners", z=3, extra={"info": infos})
    sts, infos = [], []
    for nm, rect in (("idle", (0, 300, 112, 35)), ("selected", (150, 300, 112, 35)), ("idle 3", (0, 400, 112, 35)),
                     ("selected 3", (150, 400, 112, 35))):
        st, info = pill(cg, op, rect, nm)
        sts.append(st)
        infos.append(info)
        print(nm, info)
    how = ("the taller pill: the same half-ellipse-ended pill (idle 25 px tall, selected 27 px, each state measured on its "
           "own cell), its line lit (grey-lilac along the top and upper left, navy below and right on the idle cells; a "
           "linear paint fitted to the line), row-stop fill with the left end's highlight, fitted cast shadow, the navy rods "
           "above and below")
    F.deliver("editor-pill-tall", sts, how, "lighting baked (row stops, end highlight, lit outline, cast shadow); ends are half "
              "ellipses; the lilac 'step' of the first round is gone (it is the backdrop seen through the cell, kept as the "
              "background)", z=3, extra={"info": infos})


if __name__ == "__main__":
    main()
