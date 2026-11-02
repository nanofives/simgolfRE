"""S3.29 (U-1309 follow-up): coverage ownership of Windows' generic GL on 29_probes.py's iso_cov scenes (constant-colour textures, one
colour per triangle class, so the real frame names each pixel's owner up to the colour). The model draws the triangles in order with a
row-span rule; a pixel's model colour is its last owner's. Rules:
  exact       GlRaster's (S3.1b): exact edge functions, the +y / +x tie ownership
  walk:<F>:<q>   each edge's x at the row centres walked from the edge's upper vertex (x0 at the first row exact, rounded to 2^-F
              by q, then + dx/dy per row held to 2^-F by q) and the span [ceil(xl - 0.5), ceil(xr - 0.5) - 1]
Prints the differing pixels per rule and, for `exact`, where they lie (distance to the nearest edge of the owner the real chose).

    cd <tree> && ~/bin/py -3.12 ../../../log/roadmap/S3/29_covfit.py [scene ...]
"""
import importlib
import math
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
TF = importlib.import_module("29_texfit")
PR = importlib.import_module("29_probes")


def tex_of_pass(d):
    """the bound texture index (0..7) of each triangle, in pass order"""
    out, cur = [], None
    names = {}
    for ln in (d / "stream.gl.txt").read_text().splitlines():
        if ln.startswith("glGenTextures"):
            names[int(ln.split("ids=")[1], 16)] = len(names)
    for ln in (d / "snaps" / "s1" / "pass.gl.txt").read_text().splitlines():
        if ln.startswith("glBindTexture"):
            cur = names[int(ln.split(",")[1], 16)]
        elif ln.startswith("glEnd"):
            out.append(cur)
    return out


def rgb5(c):
    return tuple(int(np.floor(v * 31 / 255 + 0.5 / 16)) for v in c)


def spans_exact(t):
    return {y: (int(xs[0]), int(xs[-1])) for y, xs in TF.coverage(t)}


def spans_walk(t, F, q, start="exact"):
    pts = [v["p"] for v in t]
    o = sorted(range(3), key=lambda i: (pts[i][1], i))
    bot, mid, top = [pts[i] for i in o]
    out = {}
    if top[1] == bot[1]:
        return out
    S = 2.0 ** F
    Q = {"floor": math.floor, "round": lambda z: math.floor(z + 0.5), "trunc": math.trunc, "ceil": math.ceil}[q]
    hold = lambda v: Q(v * S) / S

    def walk(a, b, rows):
        if a[1] == b[1] or not rows:
            return {}
        dxdy = (b[0] - a[0]) / (a[1] - b[1])           # x change per row walked down
        st = hold(dxdy)
        res = {}
        x = None
        for k, y in enumerate(rows):
            yc = y + 0.5
            if x is None:
                x = a[0] + dxdy * (a[1] - yc)
                x = hold(x) if start == "hold" else x
            else:
                x = x + st
            res[y] = x
        return res
    iy_hi = math.ceil(top[1] - 0.5) - 1
    iy_lo = math.ceil(bot[1] - 0.5)
    rows = list(range(iy_hi, iy_lo - 1, -1))
    longw = walk(top, bot, rows)
    up = [y for y in rows if y + 0.5 >= mid[1]]
    lo = [y for y in rows if y + 0.5 < mid[1]]
    s1, s2 = walk(top, mid, up), walk(mid, bot, lo)
    for y in rows:
        xa = longw.get(y)
        xb = s1.get(y, s2.get(y))
        if xa is None or xb is None:
            continue
        xl, xr = min(xa, xb), max(xa, xb)
        a_, b_ = math.ceil(xl - 0.5), math.ceil(xr - 0.5) - 1
        if b_ >= a_:
            out[y] = (a_, b_)
    return out


def spans_walkf(t, first="f32", step="f32", span="std"):
    """edges walked in float32: x at the first row centre = f32(x_top + dxdy * (y_top - yc)) ('f32'; 'exact' unrounded), then
    x = f32(x + f32(dxdy)) per row; rows and span ends as spans_walk (span 'std': [ceil(xl - 0.5), ceil(xr - 0.5) - 1])"""
    F = TF.f32
    pts = [v["p"] for v in t]
    o = sorted(range(3), key=lambda i: (pts[i][1], i))
    bot, mid, top = [pts[i] for i in o]
    out = {}
    if top[1] == bot[1]:
        return out

    def walk(a, b, rows):
        if a[1] == b[1] or not rows:
            return {}
        if step == "f32x":   # dx/dy itself in float32 arithmetic
            dxdy = F(F(F(b[0]) - F(a[0])) / F(F(a[1]) - F(b[1])))
        else:
            dxdy = F((b[0] - a[0]) / (a[1] - b[1]))
        res = {}
        x = None
        for y in rows:
            yc = y + 0.5
            if x is None:
                if first == "exact":
                    x = a[0] + (b[0] - a[0]) / (a[1] - b[1]) * (a[1] - yc)
                elif first == "f32x":
                    x = F(F(a[0]) + F(dxdy * F(F(a[1]) - F(yc))))
                else:
                    x = F(a[0] + (b[0] - a[0]) / (a[1] - b[1]) * (a[1] - yc))
            else:
                x = F(F(x) + dxdy)
            res[y] = float(x)
        return res
    iy_hi = math.ceil(top[1] - 0.5) - 1
    iy_lo = math.ceil(bot[1] - 0.5)
    rows = list(range(iy_hi, iy_lo - 1, -1))
    longw = walk(top, bot, rows)
    up = [y for y in rows if y + 0.5 >= mid[1]]
    lo = [y for y in rows if y + 0.5 < mid[1]]
    s1, s2 = walk(top, mid, up), walk(mid, bot, lo)
    for y in rows:
        xa = longw.get(y)
        xb = s1.get(y, s2.get(y))
        if xa is None or xb is None:
            continue
        xl, xr = min(xa, xb), max(xa, xb)
        if span == "std":
            a_, b_ = math.ceil(xl - 0.5), math.ceil(xr - 0.5) - 1
        else:   # GlRaster's tie side: xl < x + 0.5 <= xr
            a_, b_ = math.floor(xl - 0.5) + 1, math.floor(xr - 0.5)
        if b_ >= a_:
            out[y] = (a_, b_)
    return out


def spans_unsnapped(t, form):
    t2 = [dict(v, p=TF.unsnapped(*v["raw"], form)) for v in t]
    return {y: (int(xs[0]), int(xs[-1])) for y, xs in TF.coverage(t2)}


def render(tris, texid, rule):
    own = -np.ones((600, 800), np.int32)
    for ti, t in enumerate(tris):
        sp = rule(t)
        for y, (a, b) in sp.items():
            a, b = max(0, a), min(799, b)
            if 0 <= y < 600 and b >= a:
                own[y, a:b + 1] = ti
    return own


def main():
    for scene in sys.argv[1:] or ["s329_iso_cov", "s329_iso_cov_z0"]:
        d = TF.P.PROBE / scene
        tris, tex, real = TF.load(scene)
        tids = tex_of_pass(d)
        R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1)
        # the real owner's texture class per pixel: the class whose colour (5-bit scale) is within one level in every channel
        rclass = -np.ones((600, 800), np.int32)
        for k, c in enumerate(PR.ISO_COV_COLOURS):
            e = np.array(c, float) * 31 / 255
            m = (np.abs(R5 - e[None, None, :]) < 1.0).all(-1)
            rclass[m] = k
        rules = {"exact": spans_exact, "unsnapped_d": lambda t: spans_unsnapped(t, "d"), "unsnapped_f": lambda t: spans_unsnapped(t, "f")}
        for fi in ("f32", "exact", "f32x"):
            for st in ("f32", "f32x"):
                for sp in ("std", "gl"):
                    rules[f"walkf:{fi}:{st}:{sp}"] = (lambda t, fi=fi, st=st, sp=sp: spans_walkf(t, fi, st, sp))
        unk = int((rclass == -1).sum() - (real == 0).sum())
        print(scene, "pixels whose colour names no class:", unk)
        for nm, rule in rules.items():
            own = render(tris, tids, rule)
            mclass = np.where(own >= 0, np.array(tids + [-1])[own], -1)
            print(f"  {nm:16s} differing owners {int((mclass != rclass).sum())}", flush=True)


if __name__ == "__main__":
    main()
