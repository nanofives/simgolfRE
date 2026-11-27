"""P41 round 7: one structure per button across its states.

audit(el): compares every state's scene with the idle one, in cell coordinates: item and layer counts, shape types in
order, and per item the translation that best maps idle's points onto the state's (median of the point differences,
then rounded to whole pixels, since a state that the art moves moves by whole pixels), and what is left after it: the
largest point / radius difference. Paints are not compared.

unify(el, ref="idle"): every state rebuilt from the reference state's shapes, each item moved by its whole-pixel
translation, with the state's own paints (their positional points moved by the same amount the layer's shape moved).

Usage: py -3.12 fix7_audit.py [element ...]   prints the audit table
"""
import copy
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402

SCN = os.path.join(C.ROOT, "re", "tools", "uivec", "scenes", "hud_panels")
PT_KEYS = ("c", "a", "b", "p")
SCALAR_KEYS = ("r", "rx", "ry", "w", "d", "rot", "L")


def approved(el):
    return json.load(open(os.path.join(SCN, el + ".json")))


def walk(node, pts, sc, path=""):
    """points (x, y) and scalars of a shape tree, in a fixed order"""
    if isinstance(node, list):
        for i, n in enumerate(node):
            walk(n, pts, sc, f"{path}[{i}]")
        return
    if not isinstance(node, dict):
        return
    t = node.get("t")
    for k in sorted(node):
        v = node[k]
        if k in PT_KEYS and isinstance(v, list) and len(v) == 2 and all(isinstance(q, (int, float)) for q in v):
            if not (k == "c" and t == "half"):
                pts.append((f"{path}.{k}", float(v[0]), float(v[1])))
        elif k == "p" and isinstance(v, list) and v and isinstance(v[0], list):
            for i, q in enumerate(v):
                pts.append((f"{path}.p{i}", float(q[0]), float(q[1])))
        elif k == "pts":
            for i, q in enumerate(v):
                pts.append((f"{path}.pts{i}", float(q[0]), float(q[1])))
                for j, s in enumerate(q[2:]):
                    sc.append((f"{path}.pts{i}.{j}", float(s)))
        elif k == "rings":
            for a, r in enumerate(v):
                for i, q in enumerate(r):
                    pts.append((f"{path}.ring{a}.{i}", float(q[0]), float(q[1])))
        elif k == "half" and isinstance(v, list):
            sc += [(f"{path}.half0", float(v[0])), (f"{path}.half1", float(v[1]))]
        elif k in SCALAR_KEYS and isinstance(v, (int, float)):
            sc.append((f"{path}.{k}", float(v)))
        elif isinstance(v, (dict, list)) and k not in ("fill",):
            walk(v, pts, sc, f"{path}.{k}")


def types(node, out):
    if isinstance(node, list):
        for n in node:
            types(n, out)
    elif isinstance(node, dict):
        if "t" in node:
            out.append(node["t"])
        for k, v in node.items():
            if isinstance(v, (dict, list)) and k != "fill":
                types(v, out)
    return out


def cell_rel(state):
    x, y = state["cell"][:2]
    return K.shift(state["scene"], -x, -y)


def layer_geom(L):
    pts, sc = [], []
    walk(L["shape"], pts, sc)
    if "clip" in L:
        walk(L["clip"], pts, sc, ".clip")
    return pts, sc


def item_compare(a, b):
    """a, b: items (cell coordinates). -> (same_structure, (tx, ty) float, (ix, iy) whole px, residual pts, residual r)"""
    if len(a["layers"]) != len(b["layers"]):
        return False, None, None, None, None
    pa, pb, sa, sb = [], [], [], []
    for La, Lb in zip(a["layers"], b["layers"]):
        if types(La["shape"], []) != types(Lb["shape"], []):
            return False, None, None, None, None
        p1, s1 = layer_geom(La)
        p2, s2 = layer_geom(Lb)
        if len(p1) != len(p2) or len(s1) != len(s2):
            return False, None, None, None, None
        pa += p1
        pb += p2
        sa += s1
        sb += s2
    if not pa:
        return True, (0.0, 0.0), (0, 0), 0.0, 0.0
    A = np.array([[x, y] for _, x, y in pa])
    B = np.array([[x, y] for _, x, y in pb])
    d = B - A
    t = np.median(d, axis=0)
    it = np.round(t)
    res = float(np.abs(d - it).max())
    rs = float(max((abs(x[1] - y[1]) for x, y in zip(sa, sb)), default=0.0))
    return True, (float(t[0]), float(t[1])), (int(it[0]), int(it[1])), res, rs


def audit(el, ref="idle"):
    doc = approved(el)
    sts = doc["states"]
    if ref not in sts:
        ref = list(sts)[0]
    base = cell_rel(sts[ref])
    rows = []
    for st, v in sts.items():
        if st == ref:
            continue
        s = cell_rel(v)
        if len(s) != len(base):
            rows.append((st, "items %d vs %d" % (len(s), len(base)), []))
            continue
        per = []
        for i, (a, b) in enumerate(zip(base, s)):
            ok, t, it, res, rs = item_compare(a, b)
            per.append((i, ok, t, it, res, rs, len(a["layers"]), len(b["layers"])))
        rows.append((st, "ok" if all(p[1] for p in per) else "structure differs", per))
    return ref, rows


# ------------------------------------------------------------------------------------------------------------ unify
def _shift_fill(f, dx, dy):
    if not isinstance(f, dict) or not (dx or dy):
        return copy.deepcopy(f)
    return K.shift(f, dx, dy)


def _layer_delta(Lref, Lst, it):
    """how far the state's own layer sits from where the reference layer lands (ref + whole-px move): its paint is moved
    by minus that, so the paint keeps its place on the shape"""
    p1, _ = layer_geom(Lref)
    p2, _ = layer_geom(Lst)
    if not p1 or len(p1) != len(p2):
        return 0.0, 0.0
    A = np.array([[x, y] for _, x, y in p1]) + np.array(it)
    B = np.array([[x, y] for _, x, y in p2])
    d = np.median(A - B, axis=0)
    return float(d[0]), float(d[1])


def unify(el, ref="idle", moves=None, doc=None):
    """-> {state: scene} in sheet coordinates. moves: {state: {item index: (dx, dy)}} overrides the measured whole-px
    translations (e.g. (0, 0) for a stub that the art does not move)."""
    doc = doc or approved(el)
    sts = doc["states"]
    base = cell_rel(sts[ref])
    out = {}
    for st, v in sts.items():
        x, y = v["cell"][:2]
        if st == ref:
            out[st] = copy.deepcopy(v["scene"])
            continue
        s = cell_rel(v)
        new = []
        for i, (a, b) in enumerate(zip(base, s)):
            ok, t, it, res, rs = item_compare(a, b)
            if not ok:
                raise ValueError(f"{el} {st} item {i}: structure differs")
            if moves and st in moves and i in moves[st]:
                it = moves[st][i]
            item = {"box": [a["box"][0] + it[0], a["box"][1] + it[1], a["box"][2] + it[0], a["box"][3] + it[1]],
                    "layers": []}
            for La, Lb in zip(a["layers"], b["layers"]):
                L = {k: copy.deepcopy(val) for k, val in Lb.items() if k not in ("shape", "clip", "fill")}
                L["shape"] = K.shift(La["shape"], *it)
                if "clip" in La:
                    L["clip"] = K.shift(La["clip"], *it)
                dx, dy = _layer_delta(La, Lb, it)
                L["fill"] = _shift_fill(Lb["fill"], dx, dy)
                if "name" in La:
                    L["name"] = La["name"]
                item["layers"].append(L)
            for k in a:
                if k not in ("box", "layers"):
                    item[k] = copy.deepcopy(b.get(k, a[k]))
            new.append(item)
        out[st] = K.shift(new, x, y)
    return out


MULTI = ["golfer-club-button", "golfer-crossed-button", "golfer-trophy-button", "employee-hire", "employee-fire",
         "employee-resume", "employee-thumbs-up", "golfer-stats-button", "joecool-stats-button", "golfer-tab-info",
         "golfer-tab-mood", "golfer-tab-swing", "golfer-tab-photo", "golfer-tab-story", "golfer-tab-ok",
         "golfer-tab-history", "golfer-tab-thought", "employee-pedestal", "shot-shape-low", "shot-shape-rising",
         "shot-shape-flat", "shot-shape-high", "shot-shape-straight", "employee-page-left", "employee-page-right",
         "member-page-left", "member-page-right"]

if __name__ == "__main__":
    for el in sys.argv[1:] or MULTI:
        ref, rows = audit(el)
        for st, verdict, per in rows:
            desc = []
            for i, ok, t, it, res, rs, na, nb in per:
                if not ok:
                    desc.append(f"#{i}: layers {na} vs {nb} or types differ")
                else:
                    desc.append(f"#{i}: t ({t[0]:+.2f},{t[1]:+.2f}) -> ({it[0]:+d},{it[1]:+d}) res {res:.2f} dr {rs:.2f}")
            print(f"{el:24s} {st:10s} vs {ref}: {verdict}; " + "; ".join(desc))
