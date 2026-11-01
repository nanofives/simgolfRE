"""S3.24: the overlay line's coverage rule against Windows' generic GL (re/tools/glexact/glreplay.exe).

Data: SG_OVERLAY_DUMP=<file> on a build of the S3.24 tree makes overlay::draw_line append one record per drawn call (its seven
arguments, the GL line width used, flip, glOrtho extent, frame width, its box and the box's pixels before and after). This script

    read <dump>                          summary of a dump
    real <dump> <out.npz>                replays every distinct record through the real GL (records packed into scenes so that
                                         their boxes do not touch) and stores (pre, port post, real post) per record
    cmp <out.npz> [--rule R]             the Python model (rule R) against the real GL per record; R = s38 is the port's S3.8 model
    synth <name> <out.npz>               a synthetic scene (lines + pre) through the real GL, stored as records

The Python model reproduces overlay::draw_line's arithmetic (double, the same operation order); `--rule` selects candidate
coverage rules (see RULES).
"""
from __future__ import annotations

import argparse
import hashlib
import pathlib
import struct
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import glprobe as P  # noqa: E402

W, H = 800, 600
INIT = ["glHint 0xc50,0x1101", "glEnable 0xb20", "glHint 0xc52,0x1102", "glEnable 0xde1", "glEnable 0xb50", "glFrontFace 0x900",
        "glEnable 0xb44", "glViewport 0x0,0x0,0x320,0x258"]
K = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])


# ------------------------------------------------------------------------------------------------ dump
def read_dump(path):
    b = pathlib.Path(path).read_bytes()
    out, o = [], 0
    while o < len(b):
        h = struct.unpack_from("<17i", b, o)
        assert h[0] == 0x314C564F, (o, hex(h[0]))
        o += 68
        x0, y0, x1, y1 = h[13:17]
        n = (x1 - x0 + 1) * (y1 - y0 + 1)
        pre = np.frombuffer(b, "<u2", n, o).reshape(y1 - y0 + 1, x1 - x0 + 1).copy()
        o += 2 * n
        post = np.frombuffer(b, "<u2", n, o).reshape(y1 - y0 + 1, x1 - x0 + 1).copy()
        o += 2 * n
        wf = struct.unpack("<f", struct.pack("<i", h[8]))[0]
        out.append(dict(x1=h[1], y1=h[2], x2=h[3], y2=h[4], colour=h[5] & 0xFFFFFFFF, width=h[6], alpha=h[7], lw=wf, flip=h[9],
                        sw=h[10], sh=h[11], fw=h[12], box=(x0, y0, x1, y1), pre=pre, post=post))
    return out


def rec_key(r):
    m = hashlib.sha1()
    m.update(struct.pack("<4iI5i", r["x1"], r["y1"], r["x2"], r["y2"], r["colour"], r["width"], r["alpha"], r["flip"], *r["box"][:2]))
    m.update(struct.pack("<f", r["lw"]))
    m.update(r["pre"].tobytes())
    return m.hexdigest()


# ------------------------------------------------------------------------------------------------ real GL
def fbits(x: float) -> str:
    return "0x%x" % struct.unpack("<I", struct.pack("<f", x))[0]


def chan(colour):
    return [float(np.float32(v) / np.float32(255.0)) for v in ((colour >> 7) & 0xF8, (colour >> 2) & 0xF8, (colour << 3) & 0xF8)]


def calls(r):
    """Terrain::drawLine's GL calls; glLineWidth with the width the port used (the call's own, or the kept one for width <= 0)"""
    flip = bool(r["flip"])
    o = [P.db(0), P.db(r["sw"]), P.db(0), P.db(r["sh"])] if flip else [P.db(0), P.db(r["sw"]), P.db(r["sh"]), P.db(0)]
    a = float(np.float32(r["alpha"]) / np.float32(10.0))
    return ["glMatrixMode 0x1701", "glPushMatrix", "glLoadIdentity", "glOrtho " + ",".join(o + [P.db(-1), P.db(1)]),
            "glMatrixMode 0x1700", "glPushMatrix", "glLoadIdentity", "glDisable 0xb50", "glDisable 0xde1", "glEnable 0xbe2",
            "glBlendFunc 0x302,0x303", "glLineWidth " + fbits(r["lw"]),
            "glColor4f " + ",".join(fbits(v) for v in chan(r["colour"]) + [a]),
            "glBegin 0x1", "glVertex2i 0x%x,0x%x" % (r["x1"] & 0xFFFFFFFF, r["y1"] & 0xFFFFFFFF),
            "glVertex2i 0x%x,0x%x" % (r["x2"] & 0xFFFFFFFF, r["y2"] & 0xFFFFFFFF), "glEnd",
            "glEnable 0xb50", "glEnable 0xde1", "glDisable 0xbe2", "glPopMatrix", "glMatrixMode 0x1701", "glPopMatrix",
            "glMatrixMode 0x1700", "glFlush"]


def pack(recs, gap=3):
    """greedy: scenes of records whose boxes (grown by gap) do not intersect"""
    scenes = []
    for i, r in enumerate(recs):
        x0, y0, x1, y1 = r["box"]
        for sc in scenes:
            if all(x0 > b[2] + gap or x1 < b[0] - gap or y0 > b[3] + gap or y1 < b[1] - gap for b in sc["boxes"]):
                sc["boxes"].append(r["box"])
                sc["idx"].append(i)
                break
        else:
            scenes.append({"boxes": [r["box"]], "idx": [i]})
    return scenes


def run_real(recs, name="s324"):
    """the real GL's box after each record (each record alone over its own pre)"""
    real = [None] * len(recs)
    for k, sc in enumerate(pack(recs)):
        s = P.Scene()
        s.state += INIT
        pre = np.zeros((H, W), dtype="<u2")
        for i in sc["idx"]:
            r = recs[i]
            x0, y0, x1, y1 = r["box"]
            pre[y0:y1 + 1, x0:x1 + 1] = r["pre"]
            s.pass_ += calls(r)
        d = s.write(f"{name}_{k}")
        (d / "snaps" / "s1" / "dib_pre.bin").write_bytes(pre.tobytes())
        got = P.real_frame(d).reshape(H, W)
        mask = np.zeros((H, W), bool)
        for i in sc["idx"]:
            x0, y0, x1, y1 = recs[i]["box"]
            real[i] = got[y0:y1 + 1, x0:x1 + 1].copy()
            mask[y0:y1 + 1, x0:x1 + 1] = True
        outside = int(((got != pre) & ~mask).sum())
        if outside:
            print(f"scene {k}: {outside} px changed outside every box", flush=True)
        print(f"scene {k}: {len(sc['idx'])} records", flush=True)
    return real


# ------------------------------------------------------------------------------------------------ model
def snap(v, c, h):
    return np.floor(256.0 * (c + (v - c) * (1.0 - 1.0 / 256.0 / h)) + 0.5) / 256.0


def clip(ax, ay, bx, by, w, h):
    t0, t1 = 0.0, 1.0
    dx, dy = bx - ax, by - ay
    for p, q in ((-dx, ax), (dx, w - ax), (-dy, ay), (dy, h - ay)):
        if p == 0.0:
            if q < 0.0:
                return None
            continue
        r = q / p
        if p < 0.0:
            t0 = max(t0, r)
        else:
            t1 = min(t1, r)
    if t0 > t1:
        return None
    return ax + t0 * dx, ay + t0 * dy, ax + t1 * dx, ay + t1 * dy


def geometry(r):
    """window-space ends after clip and snap, the half width, (or None)"""
    fw, fh = r["fw"], (r.get("fh") or 600)
    sx, sy = fw / r["sw"], fh / r["sh"]
    ax, bx = r["x1"] * sx, r["x2"] * sx
    if r["flip"]:
        ay, by = r["y1"] * sy, r["y2"] * sy
    else:
        ay, by = (r["sh"] - float(r["y1"])) * sy, (r["sh"] - float(r["y2"])) * sy
    c = clip(ax, ay, bx, by, fw, fh)
    if c is None:
        return None
    ax, ay, bx, by = c
    cx, cy = fw / 2.0, fh / 2.0
    ax, bx, ay, by = snap(ax, cx, cx), snap(bx, cx, cx), snap(ay, cy, cy), snap(by, cy, cy)
    wd = min(float(r["lw"]), 10.0)
    return ax, ay, bx, by, wd / 2.0


def coverage(r, rule="s38"):
    """coverage per pixel of the record's box (0 where nothing is drawn), plus (t, n, L, ux, uy) grids"""
    x0, y0, x1, y1 = r["box"]
    g = geometry(r)
    shape = (y1 - y0 + 1, x1 - x0 + 1)
    if g is None:
        return np.zeros(shape), None
    ax, ay, bx, by, half = g
    dx, dy = bx - ax, by - ay
    L = np.sqrt(dx * dx + dy * dy)
    if L == 0.0:
        return np.zeros(shape), None
    ux, uy = dx / L, dy / L
    Y, X = np.mgrid[y0:y1 + 1, x0:x1 + 1].astype(float)
    px, py = X + 0.5 - ax, Y + 0.5 - ay
    t = px * ux + py * uy
    n = -px * uy + py * ux
    cov = np.clip(half + 0.5 - np.abs(n), 0, 1) * np.clip(t + 0.5, 0, 1) * np.clip(L - t + 0.5, 0, 1)
    geo = dict(rec=r, t=t, n=n, L=L, ux=ux, uy=uy, ax=ax, ay=ay, bx=bx, by=by, half=half, X=X, Y=Y)
    cov = RULES[rule](cov, geo)
    return cov, geo


def blend(r, cov):
    x0, y0, x1, y1 = r["box"]
    c = r["colour"]
    cr, cg, cb = chan(c)
    ca = min(max(float(np.float32(r["alpha"]) / np.float32(10.0)), 0.0), 1.0)
    d = r["pre"].astype(np.int64)
    Y, X = np.mgrid[y0:y1 + 1, x0:x1 + 1]
    dth = (K[Y & 3, X & 3] + 0.5) / 16.0
    al = ca * cov
    out = 0
    for sh, s in ((10, cr), (5, cg), (0, cb)):
        dc = (d >> sh) & 31
        v = s * 255.0 * al + dc * 255.0 / 31.0 * (1.0 - al)
        q = np.clip(np.floor(v * 31.0 / 255.0 + dth), 0, 31).astype(np.int64)
        out = out | (q << sh)
    return np.where(cov > 0, out, d).astype("<u2")


def r_span_unr(cov, g, sgmode=0, exact=False, bits=None):
    """span rule with o from the UNROUNDED setup ends c + (v - c)(1 - 2^-8 / h) (exact: no setup at all)"""
    r = g["rec"]
    sx = r["fw"] / r["sw"]
    ax, bx = r["x1"] * sx, r["x2"] * sx
    ay, by = (r["y1"], r["y2"]) if r["flip"] else (r["sh"] - r["y1"], r["sh"] - r["y2"])
    if not exact:
        ax, bx = 400 + (ax - 400) * (1 - 1 / 256 / 400), 400 + (bx - 400) * (1 - 1 / 256 / 400)
        ay, by = 300 + (ay - 300) * (1 - 1 / 256 / 300), 300 + (by - 300) * (1 - 1 / 256 / 300)
        if bits is not None:
            q = float(1 << bits)
            ax, bx, ay, by = (np.floor(v * q + 0.5) / q for v in (ax, bx, ay, by))
    dx, dy = bx - ax, by - ay
    L = np.hypot(dx, dy)
    major = max(abs(dx), abs(dy))
    if major == 0:
        return cov
    n_px = np.floor(2.0 * g["half"] * L / major) + 2.0
    X, Y = g["X"] + 0.5, g["Y"] + 0.5
    xmaj = abs(dx) >= abs(dy)
    o = Y - (ay + (X - ax) * dy / dx) if xmaj else X - (ax + (Y - ay) * dx / dy)
    sx_, sy_ = (np.sign(dx) or 1.0), (np.sign(dy) or 1.0)
    sg = [sy_ if xmaj else sx_, sx_ * sy_ if xmaj else sx_, sx_ * sy_, 1.0, -1.0][sgmode]
    s_ = o * sg
    return np.where((s_ >= -n_px / 2) & (s_ < n_px / 2), cov, 0.0)


def r_span_incl(cov, g, eps):
    """span rule that never cuts a tie: keep |o| <= N/2 + eps"""
    dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
    major = max(abs(dx), abs(dy))
    if major == 0:
        return cov
    n_px = np.floor(2.0 * g["half"] * g["L"] / major) + 2.0
    o, _ = minor_offset(g)
    return np.where(np.abs(o) <= n_px / 2 + eps, cov, 0.0)


def r_endcount(cov, g, c, eps=2e-3):
    """rule 6 plus the b-end count: in the canonical frame (major, minor increasing; origin b; window start v_ws = u * s - N/2),
    a pixel with t > L - 1 is kept only when its index k = floor(v - v_ws) from the window start is below
    floor(v_cap(u) - v_ws + c), v_cap(u) = (0.5 * sqrt(1 + s^2) - u) / s the cap line t = L + 0.5"""
    cov = r_span_incl(cov, g, eps)
    dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
    xmaj = abs(dx) >= abs(dy)
    major, minor = (dx, dy) if xmaj else (dy, dx)
    if major == 0 or minor == 0:
        return cov
    s = abs(minor / major)
    n_px = np.floor(2.0 * g["half"] * g["L"] / abs(major)) + 2.0
    X, Y = g["X"] + 0.5, g["Y"] + 0.5
    Mc, mc = (X, Y) if xmaj else (Y, X)
    bM, bm = (g["bx"], g["by"]) if xmaj else (g["by"], g["bx"])
    u = (Mc - bM) * np.sign(major)
    v = (mc - bm) * np.sign(minor)
    v_ws = u * s - n_px / 2
    v_cap = (0.5 * np.sqrt(1 + s * s) - u) / s
    k = np.floor(v - v_ws)
    drop = (g["t"] > g["L"] - 1) & (k >= np.floor(v_cap - v_ws + c))
    return np.where(drop, 0.0, cov)


def r_s38(cov, g):
    return cov


def minor_offset(g):
    """(o, xmaj): o = the pixel centre's minor coordinate minus the line centre's at the pixel centre's major coordinate"""
    dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
    X, Y = g["X"] + 0.5, g["Y"] + 0.5
    if abs(dx) >= abs(dy):
        return Y - (g["ay"] + (X - g["ax"]) * dy / dx), True
    return X - (g["ax"] + (Y - g["ay"]) * dx / dy), False


def r_span(cov, g, tie_low=True):
    """per major-axis column, N = floor(w * L / |major|) + 2 pixels centred on the line, half-open [-N/2, N/2)"""
    dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
    major = max(abs(dx), abs(dy))
    if major == 0:
        return cov
    n_px = np.floor(2.0 * g["half"] * g["L"] / major) + 2.0
    o, _ = minor_offset(g)
    keep = (o >= -n_px / 2) & (o < n_px / 2) if tie_low else (o > -n_px / 2) & (o <= n_px / 2)
    return np.where(keep, cov, 0.0)


def r_span_dir(cov, g, both=False):
    """r_span with the half-open side set by the minor-axis direction: s = o * sign(minor delta) in [-N/2, N/2); both: the
    minor delta of the ends ordered so the major coordinate increases (s = o * sign(dx) * sign(dy))"""
    dx, dy = g["bx"] - g["ax"], g["by"] - g["ay"]
    major = max(abs(dx), abs(dy))
    if major == 0:
        return cov
    n_px = np.floor(2.0 * g["half"] * g["L"] / major) + 2.0
    o, xmaj = minor_offset(g)
    sg = np.sign(dy if xmaj else dx) or 1.0
    if both and (xmaj or both == 2):
        sg = (np.sign(dx) or 1.0) * (np.sign(dy) or 1.0)
    s_ = o * sg
    return np.where((s_ >= -n_px / 2) & (s_ < n_px / 2), cov, 0.0)


RULES = {**{f"ec{c}": (lambda cv, g, c=c: r_endcount(cv, g, c / 100)) for c in (30, 35, 38, 40, 41, 42, 43, 45, 50)},
         "incl0": lambda c, g: r_span_incl(c, g, 1e-9), "incl2": lambda c, g: r_span_incl(c, g, 2e-3),
         "incl4": lambda c, g: r_span_incl(c, g, 1 / 256), **{f"b{b}s{m}": (lambda c, g, b=b, m=m: r_span_unr(c, g, m, False, b)) for b in (4, 6, 8, 10, 12, 16, 20) for m in range(5)},
         "unr0": lambda c, g: r_span_unr(c, g, 0), "unr1": lambda c, g: r_span_unr(c, g, 1), "unr2": lambda c, g: r_span_unr(c, g, 2),
         "unr3": lambda c, g: r_span_unr(c, g, 3), "unr4": lambda c, g: r_span_unr(c, g, 4),
         "ex1": lambda c, g: r_span_unr(c, g, 1, True), "ex0": lambda c, g: r_span_unr(c, g, 0, True), "s38": r_s38, "span": r_span, "span_hi": lambda c, g: r_span(c, g, False), "span_dir": r_span_dir, "span_dir2": lambda c, g: r_span_dir(c, g, 2),
         "span_dir3": lambda c, g: r_span_dir(c, g, 1)}


def diff5(a, b):
    d = np.zeros(a.shape, int)
    for sh in (10, 5, 0):
        d = np.maximum(d, np.abs(((a.astype(int) >> sh) & 31) - ((b.astype(int) >> sh) & 31)))
    return d


# ------------------------------------------------------------------------------------------------ commands
def cmd_read(a):
    recs = read_dump(a.dump)
    keys = {rec_key(r) for r in recs}
    print(len(recs), "records,", len(keys), "distinct")
    ws = {}
    for r in recs:
        ws[(r["width"], r["alpha"])] = ws.get((r["width"], r["alpha"]), 0) + 1
    print("width,alpha:", sorted(ws.items(), key=lambda kv: -kv[1])[:20])


def save(out, recs, real):
    np.savez_compressed(out, n=len(recs), meta=np.array([[r["x1"], r["y1"], r["x2"], r["y2"], r["colour"], r["width"], r["alpha"],
                                                            struct.unpack("<i", struct.pack("<f", r["lw"]))[0], r["flip"], r["sw"],
                                                            r["sh"], r["fw"], *r["box"]] for r in recs], dtype=np.int64),
                        **{f"pre{i}": r["pre"] for i, r in enumerate(recs)}, **{f"post{i}": r["post"] for i, r in enumerate(recs)},
                        **{f"real{i}": real[i] for i in range(len(recs))})


def load(path):
    z = np.load(path)
    recs = []
    for i in range(int(z["n"])):
        m = z["meta"][i]
        recs.append(dict(x1=int(m[0]), y1=int(m[1]), x2=int(m[2]), y2=int(m[3]), colour=int(m[4]) & 0xFFFFFFFF, width=int(m[5]),
                         alpha=int(m[6]), lw=struct.unpack("<f", struct.pack("<i", int(m[7])))[0], flip=int(m[8]), sw=int(m[9]),
                         sh=int(m[10]), fw=int(m[11]), box=tuple(int(v) for v in m[12:16]), pre=z[f"pre{i}"], post=z[f"post{i}"],
                         real=z[f"real{i}"]))
    return recs


def cmd_real(a):
    recs = []
    seen = set()
    for p in a.dump:
        for r in read_dump(p):
            k = rec_key(r)
            if k not in seen:
                seen.add(k)
                recs.append(r)
    if a.limit:
        recs = recs[:a.limit]
    print(len(recs), "distinct records", flush=True)
    real = run_real(recs, a.name)
    save(a.out, recs, real)


def synth_recs(name, seed=1):
    """game-like drawLine calls (integer ends, widths 2 / 3 / 10, alphas 5 / 7 / 10) as records over a pre box"""
    rng = np.random.default_rng(seed)
    recs = []
    if name == "game":       # random lengths and angles, random 15-bit background
        for _ in range(600):
            w = int(rng.choice([1, 2, 2, 2, 3, 3, 5, 10]))
            x1, y1 = int(rng.integers(20, 780)), int(rng.integers(20, 580))
            L = rng.uniform(1, 60)
            a = rng.uniform(0, 2 * np.pi)
            x2, y2 = int(round(x1 + L * np.cos(a))), int(round(y1 + L * np.sin(a)))
            recs.append(dict(x1=x1, y1=y1, x2=x2, y2=y2, colour=int(rng.choice([0x7FFF, 0x6090, 0x80000000, 0x80007FFF, 0x7C00, 0x03E0])),
                             width=w, alpha=int(rng.choice([5, 7, 10])), flip=1))
    elif name == "white":    # white on black at alpha 1: coverage readable from the value
        for _ in range(600):
            w = int(rng.choice([1, 2, 3, 5, 10]))
            x1, y1 = int(rng.integers(20, 780)), int(rng.integers(20, 580))
            L = rng.uniform(1, 50)
            a = rng.uniform(0, 2 * np.pi)
            x2, y2 = int(round(x1 + L * np.cos(a))), int(round(y1 + L * np.sin(a)))
            recs.append(dict(x1=x1, y1=y1, x2=x2, y2=y2, colour=0x7FFF, width=w, alpha=10, flip=1, black=True))
    elif name == "ties":     # rational slopes with small denominators (exact ties at +-N/2), every octant, anywhere on screen
        sl = [(1, 1), (1, 2), (2, 3), (1, 3), (3, 4), (3, 5), (2, 5), (4, 5), (1, 4), (5, 8), (3, 8)]
        for _ in range(900):
            p, q = sl[int(rng.integers(len(sl)))]
            k = int(rng.integers(2, 9))
            maj, mnr = q * k, p * k
            sx, sy = int(rng.choice([-1, 1])), int(rng.choice([-1, 1]))
            dx, dy = (maj * sx, mnr * sy) if rng.integers(2) else (mnr * sx, maj * sy)
            x1, y1 = int(rng.integers(30, 770 - abs(dx))) + (abs(dx) if dx < 0 else 0), int(rng.integers(30, 570 - abs(dy))) + (abs(dy) if dy < 0 else 0)
            recs.append(dict(x1=x1, y1=y1, x2=x1 + dx, y2=y1 + dy, colour=0x7FFF, width=int(rng.choice([1, 2, 3, 5, 10])), alpha=10,
                             flip=1, black=True))
    else:
        raise KeyError(name)
    out = []
    for r in recs:
        r.update(lw=float(r["width"]), sw=W, sh=H, fw=W)
        g = geometry(r)
        if g is None:
            continue
        ax, ay, bx, by, half = g
        reach = half + 1.5 + 2
        x0 = max(0, int(np.floor(min(ax, bx) - reach)))
        xe = min(W - 1, int(np.ceil(max(ax, bx) + reach)))
        y0 = max(0, int(np.floor(min(ay, by) - reach)))
        ye = min(H - 1, int(np.ceil(max(ay, by) + reach)))
        r["box"] = (x0, y0, xe, ye)
        shp = (ye - y0 + 1, xe - x0 + 1)
        r["pre"] = np.zeros(shp, "<u2") if r.get("black") else rng.integers(0, 32768, size=shp).astype("<u2")
        cov, _ = coverage(r, "s38")
        r["post"] = blend(r, cov)
        out.append(r)
    return out


def cmd_synth(a):
    recs = synth_recs(a.name, a.seed)
    print(len(recs), "records", flush=True)
    real = run_real(recs, "s324syn_" + a.name)
    save(a.out, recs, real)


def cmd_cmp(a):
    recs = []
    for p in a.npz:
        recs += load(p)
    tot = dict(rec=0, recbad=0, px=0, one=0, beyond=0, port_one=0, port_beyond=0, model_eq_port=0)
    for r in recs:
        cov, _g = coverage(r, a.rule)
        m = blend(r, cov)
        d = diff5(m, r["real"])
        dp = diff5(r["post"], r["real"])
        tot["rec"] += 1
        tot["recbad"] += int((d > 0).any())
        tot["px"] += int((r["real"] != r["pre"]).sum())
        tot["one"] += int((d == 1).sum())
        tot["beyond"] += int((d > 1).sum())
        tot["port_one"] += int((dp == 1).sum())
        tot["port_beyond"] += int((dp > 1).sum())
        tot["model_eq_port"] += int((m == r["post"]).all())
    print(a.rule, tot)


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("read")
    p.add_argument("dump")
    p = sp.add_parser("real")
    p.add_argument("out")
    p.add_argument("dump", nargs="+")
    p.add_argument("--limit", type=int, default=0)
    p.add_argument("--name", default="s324")
    p = sp.add_parser("synth")
    p.add_argument("name")
    p.add_argument("out")
    p.add_argument("--seed", type=int, default=1)
    p = sp.add_parser("cmp")
    p.add_argument("npz", nargs="+")
    p.add_argument("--rule", default="s38")
    a = ap.parse_args()
    {"read": cmd_read, "real": cmd_real, "cmp": cmd_cmp, "synth": cmd_synth}[a.cmd](a)


if __name__ == "__main__":
    main()
