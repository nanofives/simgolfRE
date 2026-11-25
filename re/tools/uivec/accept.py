"""uivec: the acceptance pass (P41). Every component is rendered from its recipe, averaged down to the art's own 800 x 600
pixels and scored against the art inside the component's region. With --fit the STYLE of each recipe is first MEASURED on
the art and written to style.json: the lit-surface coefficients of the bubble's body, the pad's ring and well and the
disc by linear least squares over every instance at once, the specular spot and the cast shadow by coordinate descent.

Usage: py -3.12 re/tools/uivec/accept.py [--fit] [--out DIR]
Scores per component and state: mad (mean absolute channel difference), w8 / w16 (share of pixels whose worst channel is
within 8 / 16 levels), iou (glyph silhouette overlap, art against shapes). PASS needs every threshold in LIMITS.
"""
import copy
import json
import os
import sys
import time

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import recipes  # noqa: E402
import sdf      # noqa: E402
import title    # noqa: E402

# A component passes when its INSIDE has the art's colours (pixels more than a pixel away from any edge of its shapes),
# its glyph has the art's silhouette, and its rim sits where the art's does. The pixels ON the edges are reported (mad, w16)
# but not gated: the art's edges carry the blur of its own 800 x 600 rendering, which crisp shapes should not copy.
LIMITS = {"imad": 5.0, "iw16": 0.92, "iou": 0.90, "edge": 0.5}
ST = {False: "idle", True: "hover"}
COL = {False: "blue", True: "yellow"}


class Bench:
    def __init__(self):
        self.base_idx, self.base_pal, un_idx, un_pal, mo_idx, mo_pal = title.load_art()
        self.base = self.base_pal[self.base_idx].astype(float)
        self.art = {False: title.composite(self.base_idx, self.base_pal, un_idx, un_pal).astype(float),
                    True: title.composite(self.base_idx, self.base_pal, mo_idx, mo_pal).astype(float)}
        self.meas = title.measure(verbose=False)
        self.cache = {}

    def box(self, name, what, hover):
        m = self.meas[name]
        if what == "pad":
            cx, cy, a, b, _ = m[ST[hover]]["outer"]
            x0, y0, x1, y1 = cx - a - 4, cy - b - 4, cx + a + 4, cy + b + 4
        else:
            cx, cy, r = m[ST[hover]]["c"]
            x0, y0, x1, y1 = cx - 1.3 * r, cy - 1.3 * r, cx + 1.3 * r, cy + 1.3 * r
        return max(0, int(x0)), max(0, int(y0)), min(800, int(np.ceil(x1))), min(600, int(np.ceil(y1)))

    def grid(self, name, what, hover):
        x0, y0, x1, y1 = self.box(name, what, hover)
        jj, ii = np.mgrid[y0:y1, x0:x1]
        return ii + 0.5, jj + 0.5

    def region(self, name, what, hover):
        """The pixels a component is scored on (bool mask over its box)."""
        m = self.meas[name]
        x, y = self.grid(name, what, hover)
        cx, cy, r = m[ST[hover]]["c"]
        if what == "pad":
            mo = m[ST[hover]]["outer"]
            o = {"t": "ellipse", "c": mo[:2], "r": mo[2:4], "rot": mo[4]}
            return (sdf.sd(o, x, y) < 1.0) & (np.hypot(x - cx, y - cy) > r + 2.5)
        return np.hypot(x - cx, y - cy) < r + 1.5

    def render(self, name, what, hover, style, s, hires=False):
        x0, y0, x1, y1 = self.box(name, what, hover)
        img = np.kron(self.base[y0:y1, x0:x1], np.ones((s, s, 1)))
        items = title.scene(self.meas, hover, style=style, only=(name,))
        sdf.render(items, img, s, origin=(x0, y0), cache=self.cache.setdefault((name, what, s, hover), {}))
        if hires:
            return img
        h, w = y1 - y0, x1 - x0
        return img.reshape(h, s, w, s, 3).mean(axis=(1, 3))

    def target(self, name, what, hover):
        x0, y0, x1, y1 = self.box(name, what, hover)
        return self.art[hover][y0:y1, x0:x1]


def score(b, name, what, hover, style, s=6):
    v = b.render(name, what, hover, style, s)
    a = b.target(name, what, hover)
    reg = b.region(name, what, hover)
    d = np.abs(v - a)
    worst = d.max(axis=2)[reg]
    out = {"mad": float(d[reg].mean()), "w8": float((worst <= 8).mean()), "w16": float((worst <= 16).mean())}
    m = b.meas[name]
    g = m[ST[hover]]
    x, y = b.grid(name, what, hover)
    cx, cy, r = g["c"]
    if what == "pad":
        o = {"t": "ellipse", "c": g["outer"][:2], "r": g["outer"][2:4], "rot": g["outer"][4]}
        i = {"t": "ellipse", "c": g["inner"][:2], "r": g["inner"][2:4], "rot": g["inner"][4]}
        inside = reg & (np.abs(sdf.sd(o, x, y)) > 1.5) & (np.abs(sdf.sd(i, x, y)) > 1.5)
    else:
        gd = sdf.sd({"t": "poly", "rings": g["glyph"]}, x, y) if g["glyph"] else np.full(x.shape, 99.0)
        rr = np.hypot(x - cx, y - cy)
        if g.get("body"):
            inside = (np.hypot(x - g["body"][0], y - g["body"][1]) < g["body"][2] - 1.2) & (np.abs(gd) > 1.2)
        else:
            inside = (rr < r - 2.0) & (np.abs(gd) > 1.2)
        if name in title.PARTS_FORCE and g.get("shape"):
            # an approved glyph is not the art's silhouette pixel for pixel: the colour check keeps to the body around
            # both (where neither the art's glyph nor the drawn one is)
            inside &= (gd > 1.2) & (sdf.sd(g["shape"], x, y) > 1.2)
        if g.get("body"):
            out["edge"] = rim_offset(fit.lum(a), fit.lum(v), rr - r, np.degrees(np.arctan2(y - cy, x - cx)), gd)
    wi = d.max(axis=2)[inside]
    out["imad"], out["iw16"] = float(d[inside].mean()), float((wi <= 16).mean())
    if what == "button":
        core = np.hypot(x - cx, y - cy) < r * (0.78 if m["kind"] == "disc" else 0.86)
        la, lv = fit.lum(a), fit.lum(v)
        if m["kind"] == "disc" and hover:      # a yellow glyph on lilac
            ga, gv = (a[..., 0] - a[..., 2]) > 60, (v[..., 0] - v[..., 2]) > 60
        else:
            thr = (np.percentile(la[core], 3) + np.median(la[core])) / 2
            ga, gv = la < thr, lv < thr
        out["iou"] = float((ga & gv & core).sum() / max(((ga | gv) & core).sum(), 1))
    return out


def rim_offset(la, lv, d, ang, gd):
    """How far apart the navy rim sits in the art and in the shapes, in pixels: per direction sector, the centre of the
    rim's darkness across the edge (luminance below the level just inside and outside it), art against shapes; the worst
    sector's distance. Sectors the glyph reaches are skipped."""
    worst = 0.0
    bins = np.arange(-5.0, 2.01, 0.5)
    for k in range(8):
        sec = (ang >= -180 + 45 * k) & (ang < -135 + 45 * k) & (gd > 1.5)
        cen = []
        for L in (la, lv):
            prof = []
            for d0 in bins:
                mk = sec & (d >= d0 - 0.25) & (d < d0 + 0.25)
                prof.append(L[mk].mean() if mk.sum() >= 2 else np.nan)
            prof = np.array(prof)
            if np.isnan(prof).any():
                cen = []
                break
            ref = np.interp(bins, [bins[0], bins[-1]], [prof[0], prof[-1]])
            w = np.clip(ref - prof, 0, None)
            if w.sum() < 20:
                cen = []
                break
            cen.append(float((w * bins).sum() / w.sum()))
        if len(cen) == 2:
            worst = max(worst, abs(cen[0] - cen[1]))
    return worst


# ------------------------------------------------------------------------------------------------------------ measuring
def solve_bevel(rows, stops):
    """rows: list of (u, nx, ny, rgb) arrays. The bevel paint's coefficients (stops x 5 x 3) by ridge least squares."""
    A = np.concatenate([np.stack([w for row in sdf.bevel_terms(u, nx, ny, stops) for w in row], axis=1)
                        for u, nx, ny, _ in rows])
    Y = np.concatenate([rgb for _, _, _, rgb in rows])
    lam = 1e-3 * len(A)
    G = A.T @ A + lam * np.eye(A.shape[1])
    # the ridge pulls the direction terms toward 0 and the plain colour toward the mean colour
    mean = Y.mean(0)
    prior = np.zeros((A.shape[1], 3))
    prior[0::5] = mean
    X = np.linalg.solve(G, A.T @ Y + lam * prior)
    return [[[round(float(v), 2) for v in X[k * 5 + h]] for h in range(5)] for k in range(len(stops))], len(A)


def spot_alpha(sp, cx, cy, r, x, y):
    return sp[3] * np.clip(1 - np.hypot(x - (cx + sp[0] * r), y - (cy + sp[1] * r)) / max(sp[2] * r, 0.5), 0, 1) ** sp[4]


def measure_bubble_style(b, style, hover, use_spot):
    B, p = style["bubble"], style["bubble"][COL[hover]]
    rows, rims, props = [], [], []
    for name, m in b.meas.items():
        g = m[ST[hover]]
        if m["kind"] != "bubble" or not g.get("body"):
            continue
        cx, cy, r = g["c"]
        bx, by, br = g["body"]
        x, y = b.grid(name, "button", hover)
        art = b.target(name, "button", hover)
        d = np.hypot(x - bx, y - by) - br
        far = sdf.sd({"t": "poly", "rings": g["glyph"]}, x, y) > 1.3 if g["glyph"] else np.ones(x.shape, bool)
        a = spot_alpha(p["sp"], cx, cy, r, x, y)
        mask = (d < -0.6) & far
        if use_spot:      # take the spot's light out of the art before measuring the surface under it
            tgt = (art - a[..., None] * np.asarray(p["spec"], float)) / np.maximum(1 - a[..., None], 0.05)
            mask &= a < 0.8
        else:
            tgt = art
            mask &= np.hypot(x - (cx + p["sp"][0] * r), y - (cy + p["sp"][1] * r)) > 0.36 * r
        rr = np.maximum(np.hypot(x - bx, y - by), 1e-9)
        nx, ny = (x - bx) / rr, (y - by) / rr
        rows.append(((-d / (recipes.BUBBLE_L or br))[mask], nx[mask], ny[mask], tgt[mask]))
        ring = (d > 0.7) & (np.hypot(x - cx, y - cy) < r - 0.7)
        if ring.any():
            rims.append(art[ring])
        props.append(((bx - cx) / r, (by - cy) / r, br / r))
    p["shade"], n = solve_bevel(rows, recipes.BUBBLE_STOPS)
    if rims:      # a very thin rim has no whole pixel of its own: the colour measured before stays
        p["rim"] = [round(float(v), 1) for v in np.median(np.concatenate(rims), axis=0)]
    return n, props


def measure_pad_style(b, style, hover):
    S = style["pad"][ST[hover]]
    sh = style["bubble"]["sh"]
    ring_rows, well_rows = [], []
    for name, m in b.meas.items():
        if "outer" not in m:
            continue
        cx, cy, r = m[ST[hover]]["c"]
        x, y = b.grid(name, "pad", hover)
        art = b.target(name, "pad", hover)
        mo, mi = m[ST[hover]]["outer"], m[ST[hover]]["inner"]
        o = {"t": "ellipse", "c": mo[:2], "r": mo[2:4], "rot": mo[4]}
        i = {"t": "ellipse", "c": mi[:2], "r": mi[2:4], "rot": mi[4]}
        do, di = sdf.sd(o, x, y), sdf.sd(i, x, y)
        clear = (np.hypot(x - cx, y - cy) > r + 2.0) & (np.hypot(x - (cx + sh[0] * r), y - (cy + sh[1] * r)) > r * 1.3)
        mk = (do < -0.6) & (di > 0.9) & clear
        nx, ny = sdf.normal(o, x, y, do)
        ring_rows.append(((-do / recipes.RING_L)[mk], nx[mk], ny[mk], art[mk]))
        mk = (di < -0.6) & clear
        nx, ny = sdf.normal(i, x, y, di)
        well_rows.append(((-di / recipes.WELL_L)[mk], nx[mk], ny[mk], art[mk]))
    S["ring"], n1 = solve_bevel(ring_rows, recipes.RING_STOPS)
    S["well"], n2 = solve_bevel(well_rows, recipes.WELL_STOPS)
    return n1, n2


def measure_disc_style(b, style, hover):
    D = style["disc"][ST[hover]]
    rows, glyph = [], []
    for name, m in b.meas.items():
        if m["kind"] != "disc":
            continue
        g = m[ST[hover]]
        cx, cy, r = g["c"]
        x, y = b.grid(name, "button", hover)
        art = b.target(name, "button", hover)
        d = np.hypot(x - cx, y - cy) - r
        gd = sdf.sd({"t": "poly", "rings": g["glyph"]}, x, y)
        mk = (d < -0.5) & (gd > 1.3)
        n = np.maximum(np.hypot(x - cx, y - cy), 1e-9)
        rows.append(((-d / r)[mk], ((x - cx) / n)[mk], ((y - cy) / n)[mk], art[mk]))
        if (gd < -0.9).any():
            glyph.append(art[gd < -0.9])
    D["shade"], n = solve_bevel(rows, recipes.DISC_STOPS)
    if glyph:
        D["glyph"] = [round(float(v), 1) for v in np.median(np.concatenate(glyph), axis=0)]
    return n


def descend(loss, params, passes=6, label=""):
    """Coordinate descent. params: list of [container, key or index, step, lo, hi]."""
    best = loss()
    t0 = time.time()
    for n in range(passes):
        moved = 0
        for p in params:
            v0 = p[0][p[1]]
            for sign in (1, -1):
                v = float(min(max(v0 + sign * p[2], p[3]), p[4]))
                if v == v0:
                    continue
                p[0][p[1]] = v
                e = loss()
                if e < best - 1e-9:
                    best, moved, v0 = e, moved + 1, v
                    break
                p[0][p[1]] = v0
        print(f"    [{n + 1}/{passes}] {label} rms {np.sqrt(best):.3f} moved {moved} ({time.time() - t0:.0f} s)", flush=True)
        if moved == 0:
            for p in params:
                p[2] *= 0.5
    return best


def run_fit(b, style):
    bubbles = [n for n, m in b.meas.items() if m["kind"] == "bubble"]

    def mse(what, names, states, s):
        def f():
            tot, cnt = 0.0, 0
            for n in names:
                for hv in states:
                    reg = b.region(n, what, hv)
                    d = (b.render(n, what, hv, style, s) - b.target(n, what, hv))[reg]
                    tot += float((d * d).sum())
                    cnt += d.size
            return tot / cnt
        return f

    props = []
    for hover in (False, True):
        p = style["bubble"][COL[hover]]
        n, pr = measure_bubble_style(b, style, hover, use_spot=False)
        props += pr
        print(f"bubble {COL[hover]}: surface from {n} pixels, rim {p['rim']}", flush=True)
        for rnd in range(2):
            sp = [[p["sp"], k, st, lo, hi] for k, st, lo, hi in ((0, 0.04, -0.9, 0.9), (1, 0.04, -0.9, 0.9),
                                                               (2, 0.04, 0.06, 0.9), (3, 0.08, 0.0, 1.0), (4, 0.3, 0.5, 6.0))]
            descend(mse("button", bubbles, (hover,), 4), sp, passes=5, label=f"spot {COL[hover]} round {rnd + 1}")
            measure_bubble_style(b, style, hover, use_spot=True)
        p["sp"] = [round(v, 3) for v in p["sp"]]
    style["bubble"]["body"] = [round(float(v), 4) for v in np.median(np.array(props), axis=0)]
    for hover in (False, True):
        n1, n2 = measure_pad_style(b, style, hover)
        print(f"pad {ST[hover]}: ring from {n1} pixels, well from {n2}", flush=True)
        n = measure_disc_style(b, style, hover)
        print(f"disc {ST[hover]}: surface from {n} pixels", flush=True)
    B = style["bubble"]
    descend(mse("button", bubbles, (False, True), 4),
            [[B["edge"], 0, 0.2, -2.0, 2.0], [B["edge"], 1, 0.2, -2.0, 2.0],
             [B["halo"], 0, 0.02, -0.3, 0.3], [B["halo"], 1, 0.02, -0.3, 0.3], [B["halo"], 2, 0.015, -0.1, 0.25],
             [B["halo"], 3, 0.02, 0.01, 0.4], [B["halo"], 4, 0.08, 0.0, 1.0]], passes=6, label="rim radii and halo")
    B["edge"], B["halo"] = [round(v, 3) for v in B["edge"]], [round(v, 3) for v in B["halo"]]
    sh = style["bubble"]["sh"]
    pads = [n for n in bubbles if "outer" in b.meas[n]]
    descend(mse("pad", pads, (False, True), 2),
            [[sh, 0, 0.04, -0.6, 0.6], [sh, 1, 0.04, -0.6, 0.8], [sh, 2, 0.04, 0.02, 0.8], [sh, 3, 0.08, 0.0, 1.0]],
            passes=5, label="cast shadow")
    style["bubble"]["sh"] = [round(v, 3) for v in sh]
    D = style["disc"]
    descend(mse("button", [n for n, m in b.meas.items() if m["kind"] == "disc"], (False, True), 6),
            [[D, "soft", 0.15, 0.2, 3.0]], passes=4, label="disc edge")
    D["soft"] = round(D["soft"], 3)


def sheet(b, style, out, z=6):
    """Per button: the art | the shapes at the art's resolution | their difference x4 | the shapes at full resolution."""
    for hover in (False, True):
        rows = []
        for name in b.meas:
            a = b.target(name, "button", hover)
            v = b.render(name, "button", hover, style, z)
            hi = b.render(name, "button", hover, style, z, hires=True)
            up = lambda q: np.kron(q, np.ones((z, z, 1)))   # noqa: E731
            rows.append(np.concatenate([up(a), up(v), up(np.clip(np.abs(v - a) * 4, 0, 255)), hi], axis=1))
        W = max(r.shape[1] for r in rows)
        rows = [np.pad(r, ((0, 0), (0, W - r.shape[1]), (0, 0))) for r in rows]
        Image.fromarray(np.clip(np.concatenate(rows) + 0.5, 0, 255).astype(np.uint8)).save(
            os.path.join(out, f"accept_buttons_{ST[hover]}.png"))


def main():
    out = os.path.join(fit.ROOT, "log", "roadmap", "P41", title.OUT)
    if "--out" in sys.argv:
        out = sys.argv[sys.argv.index("--out") + 1]
    os.makedirs(out, exist_ok=True)
    b = Bench()
    style = copy.deepcopy(recipes.STYLE)
    if "--fit" in sys.argv:
        run_fit(b, style)
        with open(os.path.join(os.path.dirname(__file__), "style.json"), "w") as f:
            json.dump(style, f)
    rows, fails = [], 0
    todo = [(n, w) for n in b.meas for w in (("button", "pad") if "outer" in b.meas[n] else ("button",))]
    for k, (name, what) in enumerate(todo):
        for hover in (False, True):
            sc = score(b, name, what, hover, style)
            # a glyph whose look Mariano approved by eye (title.PARTS_FORCE) is not held to the silhouette overlap:
            # the number is still printed
            waived = what == "button" and name in title.PARTS_FORCE
            ok = (sc["imad"] <= LIMITS["imad"] and sc["iw16"] >= LIMITS["iw16"]
                  and (waived or sc.get("iou", 1.0) >= LIMITS["iou"]) and sc.get("edge", 0.0) <= LIMITS["edge"])
            fails += not ok
            rows.append((name, what, ST[hover], sc, ok))
        print(f"[{k + 1}/{len(todo)}] {name}.{what} " + ("PASS" if rows[-1][4] and rows[-2][4] else "FAIL"), flush=True)
    print(f"\n{'component':18s} {'state':6s} {'in mad':>6s} {'in w16':>6s} {'iou':>6s} {'rim px':>6s} | {'all mad':>7s} "
          f"{'all w16':>7s}  result")
    with open(os.path.join(out, "accept.tsv"), "w") as f:
        f.write("component\tstate\tmad\tw8\tw16\tiou\tresult\n")
        for name, what, st, sc, ok in rows:
            iou = f"{sc['iou']:.3f}" if "iou" in sc else "-"
            edge = f"{sc['edge']:.2f}" if "edge" in sc else "-"
            print(f"{name + '.' + what:18s} {st:6s} {sc['imad']:6.2f} {sc['iw16']:6.3f} {iou:>6s} {edge:>6s} | {sc['mad']:7.2f} "
                  f"{sc['w16']:7.3f}  {'PASS' if ok else 'FAIL'}")
            f.write("\t".join([name + "." + what, st, f"{sc['mad']:.2f}", f"{sc['w8']:.3f}", f"{sc['w16']:.3f}", iou,
                               "PASS" if ok else "FAIL"]) + "\n")
    print(f"\nlimits {LIMITS}: {len(rows) - fails} PASS, {fails} FAIL")
    sheet(b, style, out)


if __name__ == "__main__":
    main()
