"""uivec states (P41S, log/roadmap/P41/STATES_BRIEF.md, notes log/roadmap/P41/states/notes.md): an element's states as short lists
of ALTERATIONS of one base drawing instead of one complete scene per state.

AN ELEMENT FILE (re/tools/uivec/states/<area>/<id>.json):
  {"id", "area", "source": "approved" | "work7",
   "variants": {<variant>: {"base": <state name the base was taken from>,
                            "items": [base items; every layer has an "id"; a layer with "hidden": true is drawn only by the
                                      states that switch it on],
                            "states": {<state>: ALTERATION}}},
   "states": {<state name as the approved file names it>: {"variant": <variant>, "origin": [dx, dy]}}}
  "origin": the state's cut corner minus the base's cut corner (the base items are in the base cut's coordinates; the state's own
  scene point = base point + origin). A variant is a separate base where the art changes the figure (a check box's tick, the
  speed glyphs, a mood face); its states are alterations of that variant's base.

AN ALTERATION (every key optional; {} = the base as it is):
  "paint": {<layer id>: {"fill": P, "alpha": a, "soft": r | null, "falloff": {...} | null, "fx": [...] | null}}
            paint overrides, in the base's coordinates (applied before the layer's move); null removes the key
  "move":  [{"d": [dx, dy], "layers": [<layer id>, ...]}]   a rigid move of a named group: shape, clip, paint coordinates
            (radial c, linear a / b, a bevel's "of"), falloff centre and tone a / b all move together
  "off":   [<layer id>, ...]   base layers not drawn in this state
  "on":    [<layer id>, ...]   hidden base layers drawn in this state (a hover halo, a selected ring, a disabled veil)
  "box":   {<item index>: [x0, y0, x1, y1]}   an item's box in this state (the box bounds the item's pixels)
No geometry in an alteration: a state cannot change a shape except by moving it.

    apply(variant, state_name) -> scene items (pure: the reference the port is tested against)
    derive([(state, items), ...], base_name) -> (variant dict, report)
    check(items_a, items_b, rect) -> max abs difference in levels at 1x and 2.7x over black and white grounds

    py -3.12 re/tools/uivec/states.py derive [--area <area>] [--id <id>] [--write] [--work7]   -> log/roadmap/P41/states/derive.tsv
    py -3.12 re/tools/uivec/states.py stamp [--work7]
  (--work7: also round 7's pending drawings, log/roadmap/P41/work7; judged and installed 2026-11-27, install78.py)
"""
import copy
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sdf  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
STATES_DIR = os.path.join(HERE, "states")
OUT_DIR = os.path.join(ROOT, "log", "roadmap", "P41", "states")
PAINT_KEYS = ("fill", "alpha", "soft", "falloff", "fx")
SCALES = (1.0, 2.7)
REPLACED = 999.0   # derive.tsv's max for a state that replaces base layers by its own (never exact)
TOL_LEVELS = 1.0


# ------------------------------------------------------------------------------------------------------------ geometry walkers
def translate(S, dx, dy):
    """S moved by (dx, dy), every kind the approved scenes use (a "move" node is resolved into its child)"""
    if dx == 0 and dy == 0:
        return copy.deepcopy(S)
    t = S["t"]
    mv = lambda p: [p[0] + dx, p[1] + dy]  # noqa: E731
    o = dict(S)
    if t in ("circle", "ellipse", "rrect", "arc", "dimpled", "marks.superellipse", "marks.star"):
        o["c"] = mv(S["c"])
    elif t == "poly":
        o["rings"] = [[mv(p) for p in r] for r in S["rings"]]
    elif t in ("stroke", "marks.pline", "marks.vline"):
        o["pts"] = [mv(p) for p in S["pts"]]
    elif t == "hud_panels.limb":
        o["pts"] = [[p[0] + dx, p[1] + dy] + list(p[2:]) for p in S["pts"]]
    elif t in ("seg", "arrow"):
        o["a"], o["b"] = mv(S["a"]), mv(S["b"])
    elif t == "half":
        o["p"] = mv(S["p"])
    elif t == "qstroke":
        o["p"] = [mv(p) for p in S["p"]]
    elif t == "lathe":
        o["x"] = S["x"] + dx
        o["prof"] = [[p[0] + dy] + list(p[1:]) for p in S["prof"]]
    elif t in ("hud_panels.rrect4", "marks.heart"):
        b = S["box"]
        o["box"] = [b[0] + dx, b[1] + dy, b[2] + dx, b[3] + dy]
    elif t == "rm.arc":
        o["c"] = mv(S["c"])
        for k in ("lead", "tail"):
            if S.get(k):
                o[k] = mv(S[k])
    elif t == "reports_slabs.rpoly":
        o["verts"] = [[v[0] + dx, v[1] + dy] + list(v[2:]) for v in S["verts"]]
    elif t in ("union", "inter", "popups.sinter", "rm.smin"):
        o["of"] = [translate(q, dx, dy) for q in S["of"]]
    elif t == "sub":
        o["a"], o["b"] = translate(S["a"], dx, dy), translate(S["b"], dx, dy)
    elif t in ("grow", "outline"):
        o["of"] = translate(S["of"], dx, dy)
    elif t == "move":
        return translate(S["of"], S["d"][0] + dx, S["d"][1] + dy)
    else:
        return {"t": "move", "of": copy.deepcopy(S), "d": [dx, dy]}
    return o


def canon(S):
    """S with every "move" node resolved (two drawings of one shape, one written with a move, compare equal)"""
    t = S["t"]
    if t == "move":
        return canon(translate(S["of"], S["d"][0], S["d"][1]))
    o = dict(S)
    o.pop("fixed", None)   # fit metadata, no geometry
    if t in ("union", "inter", "popups.sinter", "rm.smin"):
        o["of"] = [canon(q) for q in S["of"]]
    elif t == "sub":
        o["a"], o["b"] = canon(S["a"]), canon(S["b"])
    elif t in ("grow", "outline"):
        o["of"] = canon(S["of"])
    return o


def _pts_scal(S, P, X, sig):
    """collects S's points (that a translation moves) into P, its other numbers into X, its structure into sig"""
    t = S["t"]
    sig.append(t)
    num = lambda v: X.append(float(v))  # noqa: E731
    if t in ("circle", "ellipse", "rrect", "arc", "dimpled", "marks.superellipse", "marks.star", "rm.arc"):
        P.append(S["c"])
        for k in sorted(S):
            if k in ("t", "c", "lead", "tail"):
                continue
            v = S[k]
            if isinstance(v, (int, float)) and not isinstance(v, bool):
                num(v)
            elif isinstance(v, list) and all(isinstance(q, (int, float)) for q in v):
                for q in v:
                    num(q)
            else:
                sig.append(json.dumps([k, v], sort_keys=True))
        if t == "rm.arc":
            for k in ("lead", "tail"):
                sig.append(k if S.get(k) else "-")
                if S.get(k):
                    P.append(S[k])
        return
    if t == "poly":
        sig.append(f"rings{[len(r) for r in S['rings']]} s{S.get('smooth', 0)}")
        for r in S["rings"]:
            P.extend(r)
        return
    if t in ("stroke", "marks.pline", "marks.vline", "hud_panels.limb"):
        sig.append(f"n{len(S['pts'])} {S.get('cap')} {S.get('closed')} s{S.get('smooth', 0)} {S.get('n')}")
        for p in S["pts"]:
            P.append(p[:2])
            for q in p[2:]:
                num(q)
        for k in ("w", "w1"):
            if k in S:
                num(S[k])
        for q in S.get("ws", ()):
            num(q)
        return
    if t in ("seg", "arrow"):
        P.extend([S["a"], S["b"]])
        for k in ("w", "hl", "hw"):
            if k in S:
                num(S[k])
        return
    if t == "half":
        P.append(S["p"])
        X.extend(float(v) for v in S["n"])
        return
    if t == "qstroke":
        P.extend(S["p"])
        X.extend(float(v) for v in S["w"])
        return
    if t == "lathe":
        sig.append(f"prof{len(S['prof'])} s{S.get('smooth', 0)}")
        for p in S["prof"]:
            P.append([S["x"], p[0]])
            X.extend(float(v) for v in p[1:])
        return
    if t in ("hud_panels.rrect4", "marks.heart"):
        b = S["box"]
        P.extend([[b[0], b[1]], [b[2], b[3]]])
        X.extend(float(v) for v in S.get("r", ()))
        return
    if t == "reports_slabs.rpoly":
        sig.append(f"v{len(S['verts'])} {json.dumps(S.get('sag', {}), sort_keys=True)}")
        for v in S["verts"]:
            P.append(v[:2])
            X.extend(float(q) for q in v[2:])
        return
    if t in ("union", "inter", "popups.sinter", "rm.smin"):
        sig.append(f"({len(S['of'])}")
        for q in S["of"]:
            _pts_scal(q, P, X, sig)
        sig.append(")")
        for k in ("r", "k"):
            if k in S:
                num(S[k])
        return
    if t == "sub":
        _pts_scal(S["a"], P, X, sig)
        _pts_scal(S["b"], P, X, sig)
        return
    if t in ("grow", "outline"):
        _pts_scal(S["of"], P, X, sig)
        num(S.get("d", S.get("w", 0.0)))
        return
    sig.append(json.dumps(S, sort_keys=True))   # an unknown kind: compared whole


def layer_geom(L):
    """a layer's geometry: {sig, P / X (points / other numbers of its shape and its bevel's "of" shape), C / CX (its clip's),
    kind (the shape's kind), soft (a soft-edged layer: a shadow or a glow), bevel (painted with a bevel)}"""
    P, X, sig, C, CX = [], [], [], [], []
    g = {"kind": None, "soft": False, "bevel": False}
    if "keep_art" in L:   # a keep item (pseudo-layer): its keep shape
        _pts_scal(canon(L["keep_art"]), P, X, sig)
        sig = ["keep"] + sig
    else:
        cs = canon(L["shape"])
        g["kind"] = cs["t"]
        _pts_scal(cs, P, X, sig)
        f = L.get("fill")
        sig.append("|of")
        g["bevel"] = isinstance(f, dict) and "bevel" in f
        if g["bevel"] and f["bevel"].get("of"):
            _pts_scal(canon(f["bevel"]["of"]), P, X, sig)
        sig.append("|clip")
        if "clip" in L:
            _pts_scal(canon(L["clip"]), C, CX, sig)
        g["soft"] = "soft" in L
    g.update(sig=tuple(sig), P=np.asarray(P, float).reshape(-1, 2), X=np.asarray(X, float),
             C=np.asarray(C, float).reshape(-1, 2), CX=np.asarray(CX, float))
    return g


def move_paint(P, dx, dy):
    if not isinstance(P, dict) or (dx == 0 and dy == 0):
        return copy.deepcopy(P)
    if "radial" in P:
        r = dict(P["radial"])
        r["c"] = [r["c"][0] + dx, r["c"][1] + dy]
        return {"radial": r}
    if "linear" in P:
        l = dict(P["linear"])
        l["a"] = [l["a"][0] + dx, l["a"][1] + dy]
        l["b"] = [l["b"][0] + dx, l["b"][1] + dy]
        return {"linear": l}
    if "bevel" in P:
        b = dict(P["bevel"])
        if b.get("of"):
            b["of"] = translate(b["of"], dx, dy)
        return {"bevel": b}
    return copy.deepcopy(P)


def move_layer(L, dx, dy, clip=True):
    """the layer moved rigidly (shape, paint coordinates, falloff centre, tone a / b; its clip too unless clip=False: a clip is
    usually the cell or ground the layer is cut by, which stays where it is when a pressed button moves)"""
    if dx == 0 and dy == 0:
        return copy.deepcopy(L)
    if "keep_art" in L:
        o = dict(L)
        o["keep_art"] = translate(L["keep_art"], dx, dy)
        return o
    o = dict(L)
    o["shape"] = translate(L["shape"], dx, dy)
    if "clip" in L and clip:
        o["clip"] = translate(L["clip"], dx, dy)
    o["fill"] = move_paint(L["fill"], dx, dy)
    if "falloff" in L:
        f = dict(L["falloff"])
        f["c"] = [f["c"][0] + dx, f["c"][1] + dy]
        o["falloff"] = f
    if "fx" in L:
        fx = []
        for e in L["fx"]:
            e = dict(e)
            if e["t"] == "tone":
                e["a"] = [e["a"][0] + dx, e["a"][1] + dy]
                e["b"] = [e["b"][0] + dx, e["b"][1] + dy]
            fx.append(e)
        o["fx"] = fx
    return o


def paint_of(L):
    """the layer's paint keys with the bevel's "of" shape taken out (that one is geometry)"""
    out = {}
    for k in PAINT_KEYS:
        if k in L:
            out[k] = L[k]
    f = out.get("fill")
    if isinstance(f, dict) and "bevel" in f and "of" in f["bevel"]:
        b = dict(f["bevel"])
        b.pop("of")
        out["fill"] = {"bevel": b}
    return out


def _close(a, b, tol=1e-6):
    if isinstance(a, dict) and isinstance(b, dict):
        return a.keys() == b.keys() and all(_close(a[k], b[k], tol) for k in a)
    if isinstance(a, (list, tuple)) and isinstance(b, (list, tuple)):
        return len(a) == len(b) and all(_close(x, y, tol) for x, y in zip(a, b))
    if isinstance(a, (int, float)) and isinstance(b, (int, float)) and not isinstance(a, bool):
        return abs(float(a) - float(b)) <= tol
    return a == b


# ------------------------------------------------------------------------------------------------------------ apply
def _flat(items):
    """[(item index, layer index or -1 for a keep / picture item, layer dict)]"""
    out = []
    for i, it in enumerate(items):
        if "layers" in it:
            for j, L in enumerate(it["layers"]):
                out.append((i, j, L))
        else:
            out.append((i, -1, it))
    return out


def apply(variant, state):
    """the state's scene: the base items with the alteration applied (hidden layers left out unless switched on)"""
    A = variant["states"][state] if isinstance(state, str) else state
    paint = A.get("paint", {})
    moves = {}
    for g in A.get("move", ()):
        for lid in g["layers"]:
            moves[lid] = (g["d"], bool(g.get("clip", False)))
    spread = A.get("spread", {})
    off = set(A.get("off", ()))
    on = set(A.get("on", ()))
    boxes = {int(k): v for k, v in A.get("box", {}).items()}
    out = []
    for i, it in enumerate(variant["items"]):
        box = boxes.get(i, it.get("box"))
        if "layers" not in it:   # keep / picture item: moved with its id's group, never painted
            lid = it.get("id")
            if lid in off or (it.get("hidden") and lid not in on):
                continue
            o = {k: v for k, v in it.items() if k not in ("id", "hidden")}
            if lid in moves:
                d = moves[lid][0]
                o = move_layer(o, d[0], d[1])
            if box is not None:
                o["box"] = list(box)
            out.append(o)
            continue
        layers = []
        for L in it["layers"]:
            lid = L.get("id")
            if lid in off or (L.get("hidden") and lid not in on):
                continue
            o = {k: v for k, v in L.items() if k not in ("id", "hidden")}
            if lid in paint:
                for k, v in paint[lid].items():
                    if v is None:
                        o.pop(k, None)
                    elif k == "fill" and isinstance(v, dict) and "bevel" in v and isinstance(o.get("fill"), dict) \
                            and "bevel" in o["fill"] and o["fill"]["bevel"].get("of"):
                        b = dict(v["bevel"])
                        b["of"] = o["fill"]["bevel"]["of"]   # the bevel's "of" shape is the base's geometry
                        o["fill"] = {"bevel": b}
                    else:
                        o[k] = copy.deepcopy(v)
            if lid in spread:   # a shadow's or glow's spread: its shape grown by s (the port subtracts s from the distance)
                o["shape"] = {"t": "grow", "of": o["shape"], "d": spread[lid]}
            if lid in moves:
                (dx, dy), mc = moves[lid]
                o = move_layer(o, dx, dy, clip=mc)
            layers.append(o)
        if layers:
            o = {k: v for k, v in it.items() if k not in ("layers", "id", "box")}
            o["box"] = list(box)
            o["layers"] = layers
            out.append(o)
    return out


def scene_of(elem, state):
    """(items, origin) of an approved state name through the element file"""
    ref = elem["states"][state]
    return apply(elem["variants"][ref["variant"]], ref.get("alt", state)), ref.get("origin", [0, 0])


# ------------------------------------------------------------------------------------------------------------ render / check
def render(items, rect, s, bg):
    """items (layers only) over a flat ground, the rectangle rect = [x, y, w, h] of scene coordinates at scale s"""
    x, y, w, h = rect
    W, H = max(1, int(math.ceil(w * s))), max(1, int(math.ceil(h * s)))
    img = np.empty((H, W, 3))
    img[:] = bg
    sdf.render([it for it in items if "layers" in it], img, s, origin=(x, y))
    return img


def check(a, b, rect, scales=SCALES):
    """max abs difference (levels) between two scenes over black and white grounds at each scale: [max at s for s in scales]"""
    out = []
    for s in scales:
        m = 0.0
        for bg in (0.0, 255.0):
            m = max(m, float(np.abs(render(a, rect, s, bg) - render(b, rect, s, bg)).max()))
        out.append(m)
    return out


# ------------------------------------------------------------------------------------------------------------ derive
GAP = 4.0          # cost of a layer only one side has
MATCH_MAX = 6.0    # largest geometric residual (px) two layers may differ by and still be the same layer


def _match_cost(gb, gs):
    """how base layer geometry gb draws state layer geometry gs: {cost, d (the shape's move), res (what is left: px), clip (the
    clip moves with the shape), spread (a soft layer's shape grown by this)}, or None when they are not the same layer"""
    if gb["sig"] != gs["sig"] or gb["P"].shape != gs["P"].shape or gb["X"].shape != gs["X"].shape \
            or gb["C"].shape != gs["C"].shape or gb["CX"].shape != gs["CX"].shape:
        return None
    Pb, Ps = gb["P"], gs["P"]
    if len(Pb):
        dv = (Ps - Pb).mean(axis=0)
        res_p = float(np.abs(Ps - Pb - dv).max())
    else:
        dv, res_p = np.zeros(2), 0.0
    dX = gs["X"] - gb["X"]
    res_x = float(np.abs(dX).max()) if len(dX) else 0.0
    spread = 0.0
    if gb["soft"] and not gb["bevel"] and res_x > 1e-9 and gb["kind"] in ("circle", "rrect"):
        # a shadow or glow drawn bigger or smaller: circle (r), rrect (half x, half y, r, rot) grown by s
        s = float(dX[0])
        want = np.array([s] if gb["kind"] == "circle" else [s, s, s, 0.0])
        r2 = float(np.abs(dX - want).max()) if len(want) == len(dX) else res_x
        if r2 < res_x:
            spread, res_x = s, r2
    clip_moves, res_c = False, 0.0
    if len(gb["C"]):
        fixed = float(np.abs(gs["C"] - gb["C"]).max())
        moved = float(np.abs(gs["C"] - gb["C"] - dv).max())
        clip_moves = moved < fixed
        res_c = min(fixed, moved)
    if len(gb["CX"]):
        res_c = max(res_c, float(np.abs(gs["CX"] - gb["CX"]).max()))
    res = max(res_p, res_x, res_c)
    if res > MATCH_MAX or float(np.hypot(*dv)) > 2 * MATCH_MAX:
        return None
    return {"cost": res + 0.05 * float(np.hypot(*dv)) + 0.05 * abs(spread), "d": dv, "res": res, "clip": clip_moves,
            "spread": spread}


def _align(B, S):
    """global alignment of the base layers B and the state layers S (lists of (geom, paint)): [(i or None, j or None, d, res)]"""
    n, m = len(B), len(S)
    INF = 1e18
    C = np.full((n + 1, m + 1), INF)
    T = np.zeros((n + 1, m + 1), np.int8)
    C[0, :] = np.arange(m + 1) * GAP
    C[:, 0] = np.arange(n + 1) * GAP
    T[0, 1:], T[1:, 0] = 2, 1
    cache = {}
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            best, bt = C[i - 1, j] + GAP, 1
            if C[i, j - 1] + GAP < best:
                best, bt = C[i, j - 1] + GAP, 2
            mc = _match_cost(B[i - 1][0], S[j - 1][0])
            if mc is not None:
                cost = mc["cost"] + (0.0 if _close(B[i - 1][1], S[j - 1][1]) else 0.01)
                if C[i - 1, j - 1] + cost <= best:
                    best, bt = C[i - 1, j - 1] + cost, 3
                    cache[(i, j)] = mc
            C[i, j], T[i, j] = best, bt
    out = []
    i, j = n, m
    while i or j:
        bt = T[i, j]
        if bt == 3:
            out.append((i - 1, j - 1, cache[(i, j)]))
            i, j = i - 1, j - 1
        elif bt == 1:
            out.append((i - 1, None, None))
            i -= 1
        else:
            out.append((None, j - 1, None))
            j -= 1
    return out[::-1]


def _role(L, k):
    """a readable layer id: what the layer looks like (shadow, glow, veil, body, rim, line, glyph, fill) and its place"""
    if "keep_art" in L:
        return f"keep{k}"
    f = L.get("fill")
    soft = "soft" in L
    a = L.get("alpha", 1.0)
    t = canon(L["shape"])["t"]
    lum = None
    if isinstance(f, list):
        lum = 0.3 * f[0] + 0.59 * f[1] + 0.11 * f[2]
    if soft and lum is not None and lum < 110:
        r = "shadow"
    elif soft:
        r = "glow"
    elif a < 1.0:
        r = "veil"
    elif isinstance(f, dict) and "bevel" in f:
        r = "body"
    elif t in ("outline", "arc"):
        r = "rim"
    elif t in ("stroke", "seg", "qstroke", "hud_panels.limb", "marks.pline", "marks.vline"):
        r = "line"
    elif t in ("poly", "arrow", "union", "sub", "lathe"):
        r = "glyph"
    else:
        r = "fill"
    return f"{r}{k}"


def _decoration(L):
    """a layer a state may switch on: soft-edged (a halo, a glow, a soft shadow), translucent (a veil) or a ring (an outline)"""
    if "keep_art" in L:
        return False
    if "soft" in L or L.get("alpha", 1.0) < 1.0:
        return True
    S = canon(L["shape"])
    if S["t"] in ("outline", "arc"):
        return True
    return S["t"] == "sub" and S["a"]["t"] in ("circle", "ellipse", "rrect") and S["b"]["t"] in ("circle", "ellipse", "rrect")


def _round_move(dv):
    """a move the art makes is whole pixels: snap components within 0.35 px of an integer"""
    return [float(round(v)) if abs(v - round(v)) <= 0.35 else round(float(v), 4) for v in dv]


def derive(states, base_name, rects, origins, unify=False):
    """states: [(name, items)] (items in each state's own cut coordinates moved by -origin, i.e. all in the base's), rects:
    {name: [x, y, w, h]} each state's cut in base coordinates; returns (variant, report {name: {...}}).
    unify=False: the alteration that reproduces each approved state as closely as the vocabulary allows (geometry drift between
    the base and the state is reported, not kept). unify=True: also snap moves to whole pixels where the art moves by pixels."""
    named = dict(states)
    base_items = copy.deepcopy(named[base_name])
    # ids on the base's layers (and keep items)
    flat_b = _flat(base_items)
    taken, count = set(), {}
    for k, (i, j, L) in enumerate(flat_b):
        nm = L.get("name")
        if isinstance(nm, str) and nm and nm not in taken:
            lid = nm
        else:
            r = _role(L, "")
            while True:
                count[r] = count.get(r, 0) + 1
                lid = f"{r}{count[r]}"
                if lid not in taken:
                    break
        taken.add(lid)
        L["id"] = lid
    variant = {"base": base_name, "items": base_items, "states": {}}
    report = {}
    pending_hidden = []   # (state, base layer index to insert before, item box, layer)
    gB = [(layer_geom(L), paint_of(L) if "keep_art" not in L else {}) for (_, _, L) in flat_b]
    for name, items in states:
        if name == base_name:
            variant["states"][name] = {}
            report[name] = {"status": "base", "geom": [], "moves": [], "on": [], "off": [], "paint": 0}
            continue
        flat_s = _flat(items)
        gS = [(layer_geom(L), paint_of(L) if "keep_art" not in L else {}) for (_, _, L) in flat_s]
        al = _align(gB, gS)
        A = {"paint": {}, "move": [], "spread": {}, "off": [], "on": [], "box": {}}
        geom, mv = [], {}
        item_votes = {}
        prev_b = -1
        for (bi, sj, m) in al:
            if bi is not None and sj is not None:
                Lb, Ls = flat_b[bi][2], flat_s[sj][2]
                dv, res = m["d"], m["res"]
                d = _round_move(dv) if unify else [round(float(v), 6) for v in dv]
                d = [0.0 if abs(v) < 1e-6 else float(v) for v in d]
                if res > 1e-6:
                    geom.append({"layer": Lb["id"], "residual": round(res, 3)})
                if d != [0.0, 0.0]:
                    mv.setdefault((d[0], d[1], m["clip"]), []).append(Lb["id"])
                if abs(m["spread"]) > 1e-9:
                    A["spread"][Lb["id"]] = round(m["spread"], 6)
                if "keep_art" not in Lb:
                    # the state's paint in the base's coordinates
                    pb, ps = paint_of(Lb), paint_of(move_layer(Ls, -d[0], -d[1]))
                    diff = {}
                    for k in PAINT_KEYS:
                        if k in ps and (k not in pb or not _close(pb[k], ps[k], 1e-9)):
                            diff[k] = ps[k]
                        elif k in pb and k not in ps:
                            diff[k] = None
                    if diff:
                        A["paint"][Lb["id"]] = diff
                ib, isj = flat_b[bi][0], flat_s[sj][0]
                item_votes.setdefault(ib, {}).setdefault(isj, 0)
                item_votes[ib][isj] += 1
                prev_b = bi
            elif bi is not None:
                A["off"].append(flat_b[bi][2]["id"])
            else:
                Ls = flat_s[sj][2]
                pending_hidden.append((name, prev_b, items[flat_s[sj][0]].get("box"), copy.deepcopy(Ls)))
        # item boxes: the box of the state item most of a base item's layers come from
        for ib, votes in item_votes.items():
            isj = max(votes, key=votes.get)
            sb = items[isj].get("box")
            if sb is not None and "box" in base_items[ib] and not _close(list(sb), list(base_items[ib]["box"]), 1e-6):
                A["box"][str(ib)] = list(sb)
        # the whole drawing moved by whole pixels (every drawn layer, clips too): that is the state's origin, not a patch
        origin_add = None
        drawn = [L["id"] for (_, _, L) in flat_b if L["id"] not in A["off"]]
        # (install78: a move group of clipped layers moving their clips and one of unclipped layers are the same rigid move)
        if mv and len({k[:2] for k in mv}) == 1 and not any(p[0] == name for p in pending_hidden):
            (mx, my, _), _ = next(iter(mv.items()))
            ids = [i for k, v in mv.items() for i in v]
            ok_clip = all(k[2] or not any("clip" in L for (_, _, L) in flat_b if L["id"] in v) for k, v in mv.items())
            if sorted(ids) == sorted(drawn) and mx == round(mx) and my == round(my) and ok_clip:
                origin_add = [float(mx), float(my)]
                mv = {}
                nb = {}
                for k, b in A["box"].items():
                    b2 = [b[0] - mx, b[1] - my, b[2] - mx, b[3] - my]
                    if not _close(b2, list(base_items[int(k)]["box"]), 1e-6):
                        nb[k] = b2
                A["box"] = nb   # (paint overrides are in the base's coordinates: the origin carries them with the drawing)
        A["move"] = [dict({"d": [k[0], k[1]], "layers": ids}, **({"clip": True} if k[2] else {})) for k, ids in mv.items()]
        variant["states"][name] = {k: v for k, v in A.items() if v}
        report[name] = {"status": "?", "geom": geom, "moves": [[list(k[:2]), len(ids)] for k, ids in mv.items()],
                        "off": list(A["off"]), "on": [], "paint": len(A["paint"]), "spread": dict(A["spread"]),
                        "origin_add": origin_add}
        # a state that leaves base layers out AND draws layers of its own replaces part of the figure: not an alteration
        own = [p[3] for p in pending_hidden if p[0] == name]
        if A["off"] and own:
            report[name]["replace"] = (len(A["off"]), len(own))
        # a state may switch on decorations (a halo, a glow, a veil, a ring); a hard opaque shape of its own (a check box's tick,
        # another glyph) changes the figure: a variant, not an alteration
        fig = [L for L in own if not _decoration(L)]
        if fig:
            report[name]["figure"] = len(fig)
    # hidden layers (only some states draw them): each its own base item after the base layer it followed, split the item there
    if pending_hidden:
        _insert_hidden(variant, pending_hidden, report)
    return variant, report


def _insert_hidden(variant, pending, report):
    items = variant["items"]
    flat = _flat(items)
    # where each hidden layer goes: after flat index prev_b (-1: before everything)
    by_pos = {}
    for k, (state, prev_b, box, L) in enumerate(pending):
        by_pos.setdefault(prev_b, []).append((state, box, L))
    new_items = []
    n_hidden = 0

    def emit_hidden(lst):
        nonlocal n_hidden
        for state, box, L in lst:
            n_hidden += 1
            lid = f"{_role(L, '')}h{n_hidden}"
            L = dict(L, id=lid, hidden=True)
            if "keep_art" in L:
                new_items.append(dict(L, **({"box": box} if box is not None else {})))
            else:
                new_items.append({"box": box, "layers": [L]})
            variant["states"][state].setdefault("on", []).append(lid)
            report[state]["on"].append(lid)

    emit_hidden(by_pos.get(-1, []))
    fi = 0
    for i, it in enumerate(items):
        if "layers" not in it:
            new_items.append(it)
            emit_hidden(by_pos.get(fi, []))
            fi += 1
            continue
        cur = []
        for L in it["layers"]:
            cur.append(L)
            if fi in by_pos:
                new_items.append(dict(it, layers=cur))
                cur = []
                emit_hidden(by_pos[fi])
            fi += 1
        if cur:
            new_items.append(dict(it, layers=cur))
    # box overrides were keyed by the old item indices: remap (an item split in parts keeps its box in every part)
    old_to_new = {}
    k = 0
    for i, it in enumerate(items):
        n_parts = 0
        nl = len(it.get("layers", [None]))
        old_to_new[i] = []
        seen = 0
        while seen < nl:
            ni = new_items[k]
            if ni.get("hidden") or (ni.get("layers") and ni["layers"][0].get("hidden")):
                k += 1
                continue
            old_to_new[i].append(k)
            seen += len(ni.get("layers", [None]))
            k += 1
            n_parts += 1
    for A in variant["states"].values():
        if "box" in A:
            nb = {}
            for ki, b in A["box"].items():
                for nk in old_to_new[int(ki)]:
                    nb[str(nk)] = b
            A["box"] = nb
    variant["items"] = new_items


# ------------------------------------------------------------------------------------------------------------ element sources
INTERACTION = ["on the backdrop", "idle", "hover", "selected", "disabled", "active", "pressed", "lit", "dim", "plain"]
BASE_ORDER = ["idle", "plain", "lit", "a", "up", "empty"]


def parse_state(name):
    """(variant key, interaction word or None) of an approved state name ("large hover" -> ("large", "hover"),
    "hover (CGButtons, yellow)" -> ("", "hover"), "face 3" -> ("face 3", None))"""
    n = name.split("(")[0].strip().lower()
    for w in INTERACTION:
        if n == w or n.startswith(w + " ") or n.endswith(" " + w) or f" {w} " in n:
            rest = (" " + n + " ").replace(" " + w + " ", " ").strip()
            return rest, w
    return n, None


def in_game_records(area, override=None):
    """{id: [(state, sheet, cut, shift, items, extra)]} of an in-game area (port_game's records, the key layers dropped);
    override: {id: path} scene files read instead of the approved ones (round 7's pending scenes)"""
    import port_game as PG
    saved = PG.load
    if override:
        def load(a, eid):
            if eid in override:
                with open(override[eid], encoding="utf-8") as f:
                    return json.load(f)
            return saved(a, eid)
        PG.load = load
    try:
        out = {}
        for eid, st, sheet, cut, shift, items, extra in PG.records(area):
            out.setdefault(eid, []).append((st, sheet, cut, shift, items, extra))
        return out
    finally:
        PG.load = saved


def frontend_records(override=None):
    """{id: [(state, None, rect, (0, 0), items, {})]} of re/tools/uivec/scenes/frontend (scene coordinates as the file's)"""
    import csv
    d = os.path.join(HERE, "scenes")
    with open(os.path.join(d, "manifest.tsv"), encoding="utf-8") as f:
        ids = [r["id"] for r in csv.DictReader(f, delimiter="\t") if r["area"] == "frontend"]
    out = {}
    for eid in ids:
        p = (override or {}).get(eid) or os.path.join(d, "frontend", eid + ".json")
        with open(p, encoding="utf-8") as f:
            j = json.load(f)
        for s in j["states"]:
            out.setdefault(eid, []).append((s["name"], None, list(s["rect"]), (0, 0), s["scene"], {}))
    return out


def title_records():
    """the title menu's six buttons (port_export.scenes: title.scene of the measured geometry, idle / hover) and the difficulty
    screen's four pads (port_export.diff_scenes), all in screen coordinates; rect = the scene's own bounds"""
    import port_export as PE
    out = {}
    for name, idx, hover, items in PE.scenes():
        out.setdefault(f"title.{name}", []).append(("hover" if hover else "idle", None, _bounds(items), (0, 0), items, {}))
    for (pad, hover), items in sorted(PE.diff_scenes().items()):
        out.setdefault(f"diff.{pad}", []).append(("hover" if hover else "idle", None, _bounds(items), (0, 0), items, {}))
    for k, v in out.items():
        r = [_bounds_union([s[2] for s in v])] * len(v)
        out[k] = [(s[0], s[1], r[i], s[3], s[4], s[5]) for i, s in enumerate(v)]
    return out


def _bounds(items):
    x0 = min(it["box"][0] for it in items)
    y0 = min(it["box"][1] for it in items)
    x1 = max(it["box"][2] for it in items)
    y1 = max(it["box"][3] for it in items)
    x0, y0 = math.floor(x0), math.floor(y0)
    return [x0, y0, math.ceil(x1) - x0, math.ceil(y1) - y0]


def _bounds_union(rects):
    x0 = min(r[0] for r in rects)
    y0 = min(r[1] for r in rects)
    x1 = max(r[0] + r[2] for r in rects)
    y1 = max(r[1] + r[3] for r in rects)
    return [x0, y0, x1 - x0, y1 - y0]


def work7_overrides(area):
    """the round-7 scenes not judged yet (log/roadmap/P41/work7/<area>/<id>.json): {id: path}"""
    d = os.path.join(ROOT, "log", "roadmap", "P41", "work7", area)
    if not os.path.isdir(d):
        return {}
    return {f[:-5]: os.path.join(d, f) for f in os.listdir(d) if f.endswith(".json") and not f.endswith("_check.json")}


# ------------------------------------------------------------------------------------------------------------ element derivation
AREAS = ["hud_palettes", "hud_panels", "popups", "reports_slabs", "reports_maps", "marks", "frontend", "title"]


def area_records(area, source="approved"):
    over = work7_overrides(area) if source == "work7" else None
    if area == "frontend":
        return frontend_records(over)
    if area == "title":
        return title_records()
    return in_game_records(area, over)


def _scene_rect(cut, shift):
    return [cut[0] - shift[0], cut[1] - shift[1], cut[2], cut[3]]


def scene_sha(items):
    """a short hash of an approved state's scene (the exporter uses a patch only while both the state's and its base's scenes still
    hash the same: an element file never outlives an edit of the approved scenes)"""
    import hashlib
    return hashlib.sha1(json.dumps(items, sort_keys=True).encode()).hexdigest()[:16]


def _moved_items(items, o):
    out = []
    for it in items:
        n = dict(it)
        if "box" in it:
            b = it["box"]
            n["box"] = [b[0] - o[0], b[1] - o[1], b[2] - o[0], b[3] - o[1]]
        if "layers" in it:
            n["layers"] = [move_layer(L, -o[0], -o[1]) for L in it["layers"]]
        elif "keep_art" in it:
            n["keep_art"] = translate(it["keep_art"], -o[0], -o[1])
        out.append(n)
    return out


UNIFIED_DIR = os.path.join(HERE, "unified")


def given_element(area, eid):
    """install78: the approved unified element of (area, eid) (re/tools/uivec/unified/<area>/<id>.json) or None"""
    p = os.path.join(UNIFIED_DIR, area, eid + ".json")
    if not os.path.exists(p):
        return None
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def derive_element(area, eid, recs, unify=False, do_check=True, given=None):
    """recs: [(state, sheet, cut, shift, items, extra)] -> (element dict, rows for derive.tsv). given: an approved unified element
    (given_element): each of its variants whose every state's scene is apply(variant, state) at its origin within TOL_LEVELS is
    used as is (Mariano approved it as base + alteration, so a state that switches a base layer for one of its own is exact)."""
    names = [r[0] for r in recs]
    by = {r[0]: r for r in recs}
    parsed = {n: parse_state(n) for n in names}
    main = names[0]
    for w in BASE_ORDER:
        c = [n for n in names if parsed[n][1] == w or parsed[n][0] == w]
        if c:
            main = c[0]
            break
    mvkey = parsed[main][0]

    def prep(base, members):
        """states moved into the base's coordinates: [(name, items)], rects, origins"""
        b = by[base]
        bitems = b[4]
        brect = _scene_rect(b[2], b[3])
        bjs = json.dumps(bitems, sort_keys=True)
        sts, rects, origins = [], {}, {}
        for n in members:
            r = by[n]
            items = r[4]
            rr = _scene_rect(r[2], r[3])
            if items is bitems or json.dumps(items, sort_keys=True) == bjs:
                o = [0.0, 0.0]   # one scene shared by several cuts (reports_slabs): the same drawing, no origin
            else:
                o = [float(rr[0] - brect[0]), float(rr[1] - brect[1])]
            sts.append((n, _moved_items(items, o) if o != [0.0, 0.0] else items))
            rects[n] = [rr[0] - o[0], rr[1] - o[1], rr[2], rr[3]]
            origins[n] = o
        return sts, rects, origins

    def run(base, members):
        sts, rects, origins = prep(base, members)
        var, rep = derive(sts, base, rects, origins, unify=unify)
        # a whole-drawing move by whole pixels went into the origin: compare in the state's own coordinates moved by the new origin
        moved = []
        for n, items in sts:
            oa = rep[n].get("origin_add")
            if oa:
                items = _moved_items(items, oa)
                rects[n] = [rects[n][0] - oa[0], rects[n][1] - oa[1], rects[n][2], rects[n][3]]
                origins[n] = [origins[n][0] + oa[0], origins[n][1] + oa[1]]
            moved.append((n, items))
        sts = moved
        res = {}
        same = json.dumps(by[base][4], sort_keys=True)
        for n, items in sts:
            if n == base or (not var["states"].get(n) and json.dumps(by[n][4], sort_keys=True) == same):
                res[n] = (0.0, 0.0)   # (the same drawing: one scene shared by several cuts)
            elif rep[n].get("replace") or rep[n].get("figure"):
                res[n] = (REPLACED, REPLACED)   # (not checked: a replacement is never an exact alteration)
            elif do_check:
                res[n] = tuple(check(apply(var, n), items, rects[n]))
            else:
                res[n] = (float("nan"), float("nan"))
        return var, rep, res, origins, sts, rects

    var, rep, res, origins, _, rects0 = run(main, names)
    variants = {"": var}
    vmap, rows, rest = {}, [], []
    for n in names:
        if n == main or max(res[n]) <= TOL_LEVELS:
            vmap[n] = ("", origins[n])
        else:
            rest.append(n)
    # states that are not an exact alteration of the main base: a variant (another figure) or a not-exact state of this one
    notexact, groups = set(), {}
    for n in rest:
        vk, w = parsed[n]
        if vk != mvkey or w is None:
            groups.setdefault(vk if w is not None else f"{vk}#{n}", []).append(n)
        else:
            notexact.add(n)
            vmap[n] = ("", origins[n])
    for gk, members in groups.items():
        gb = members[0]
        for w in BASE_ORDER:
            c = [n for n in members if parsed[n][1] == w]
            if c:
                gb = c[0]
                break
        gvar, grep, gres, gor, _, _ = run(gb, members)
        vname = gk.split("#")[0] or gb
        while vname in variants:
            vname += "'"
        variants[vname] = gvar
        for n in members:
            vmap[n] = (vname, gor[n])
            rep[n], res[n] = grep[n], gres[n]
            if n == gb:
                rep[n] = dict(grep[n], status="variant-base")
            elif max(gres[n]) > TOL_LEVELS:
                notexact.add(n)
    # each variant again from its base and its exact states only: a not-exact state or another variant's figure leaves no hidden
    # layers in the base the port draws
    for vname in list(variants):
        members = [n for n in names if vmap[n][0] == vname and n not in notexact]
        gb = variants[vname]["base"]
        fvar, frep, fres, forig, _, _ = run(gb, members)
        variants[vname] = fvar
        for n in members:
            st0 = rep[n].get("status")
            rep[n], res[n] = frep[n], fres[n]
            if st0 in ("base", "variant-base"):
                rep[n] = dict(frep[n], status=st0)
            elif max(fres[n]) > TOL_LEVELS:
                notexact.add(n)
            vmap[n] = (vname, forig[n])
        for n in names:   # (a not-exact state keeps its first report; its own scene is what the port draws)
            if vmap[n][0] == vname and n in notexact and n in fvar["states"]:
                del fvar["states"][n]
    if given:
        for gname, gv in given["variants"].items():
            members = [n for n, r in given["states"].items() if r["variant"] == gname and n in by and n in gv["states"]]
            if gv["base"] not in members:
                continue
            gres = {}
            for n in members:
                o = given["states"][n].get("origin", [0, 0])
                rr = _scene_rect(by[n][2], by[n][3])
                gres[n] = tuple(check(apply(gv, n), _moved_items(by[n][4], o), [rr[0] - o[0], rr[1] - o[1], rr[2], rr[3]]))
            if max(max(v) for v in gres.values()) > TOL_LEVELS:
                print(f"  [{area}] {eid}: the approved unified variant {gname!r} does not reproduce the scenes "
                      f"({max(max(v) for v in gres.values()):.2f} levels): derived instead", flush=True)
                continue
            vname = vmap[gv["base"]][0]
            if variants[vname]["base"] != gv["base"]:   # (derive filed the unified base under another base: a variant of its own)
                vname = gname or gv["base"]
                while vname in variants:
                    vname += "'"
                old_members = []
            else:
                old_members = [n for n in names if vmap[n][0] == vname]
            variants[vname] = copy.deepcopy(gv)
            for n in members:
                A = gv["states"][n]
                vmap[n] = (vname, list(given["states"][n].get("origin", [0, 0])))
                res[n] = gres[n]
                notexact.discard(n)
                rep[n] = {"status": ("variant-base" if vname else "base") if n == gv["base"] else "exact", "geom": [],
                          "moves": [[list(g["d"]), len(g["layers"])] for g in A.get("move", ())], "off": list(A.get("off", ())),
                          "on": list(A.get("on", ())), "paint": len(A.get("paint", {})), "spread": dict(A.get("spread", {})),
                          "given": True}
            for n in old_members:   # (a state the unified element does not name: its own scene)
                if n not in members:
                    notexact.add(n)
                    rep[n] = dict(rep[n], status="not-exact")
    elem = {"id": eid, "area": area, "variants": variants,
            "states": {n: {"variant": vmap[n][0], "origin": vmap[n][1], "sha": scene_sha(by[n][4])} for n in names}}
    if given:
        elem["unified"] = os.path.relpath(os.path.join(UNIFIED_DIR, area, eid + ".json"), ROOT).replace("\\", "/")
    for n in names:
        v = vmap[n][0]
        r = rep[n]
        st = r["status"]
        if st == "?":
            st = "not-exact" if n in notexact else "exact"
        r["status"] = st
        elem["states"][n]["status"] = st
        geo = sorted(r.get("geom", []), key=lambda g: -g["residual"])
        if r.get("replace"):
            geo = [{"layer": f"replaces {r['replace'][0]} base layers by {r['replace'][1]} own", "residual": 0.0}] + geo
        if r.get("figure"):
            geo = [{"layer": f"draws {r['figure']} opaque shapes of its own (another figure)", "residual": 0.0}] + geo
        rows.append({"area": area, "id": eid, "variant": v or "-", "base": variants[v]["base"], "state": n, "status": st,
                     "max_1x": round(res[n][0], 3), "max_2.7x": round(res[n][1], 3),
                     "paint": r.get("paint", 0), "moves": ";".join(f"{d[0]:g},{d[1]:g}x{c}" for d, c in r.get("moves", [])),
                     "spread": ",".join(f"{k}:{v:g}" for k, v in r.get("spread", {}).items()),
                     "off": ",".join(r.get("off", [])), "on": ",".join(r.get("on", [])),
                     "geometry": ";".join(f"{g['layer']}:{g['residual']:g}px" for g in geo[:8])
                     + (f";+{len(geo) - 8} more" if len(geo) > 8 else "")})
    return elem, rows


TSV_COLS = ["area", "id", "source", "variant", "base", "state", "status", "max_1x", "max_2.7x", "paint", "moves", "spread", "off", "on",
            "geometry"]


def main_derive(argv):
    import time
    areas = argv[argv.index("--area") + 1].split(",") if "--area" in argv else AREAS
    only = argv[argv.index("--id") + 1].split(",") if "--id" in argv else None
    write = "--write" in argv
    tsv = argv[argv.index("--tsv") + 1] if "--tsv" in argv else os.path.join(OUT_DIR, "derive.tsv")
    if any(a not in ("frontend", "title") for a in areas):
        import port_game as PG
        PG.load_area_kinds()
        PG._area_kinds()
    rows_all = []
    for area in areas:
        # (install78, 2026-11-27: round 7 is judged and installed in scenes/; its pending pass runs only on request)
        sources = ["approved"] + (["work7"] if "--work7" in argv and work7_overrides(area) else [])
        for source in sources:
            recs = area_records(area, source)
            w7 = set(work7_overrides(area)) if source == "work7" else set()
            for eid, rs in recs.items():
                if len(rs) < 2 or (only and eid not in only) or (source == "work7" and eid not in w7):
                    continue
                t0 = time.time()
                elem, rows = derive_element(area, eid, rs, given=given_element(area, eid) if source == "approved" else None)
                elem["source"] = source if source == "approved" else "pending-round-7"
                for r in rows:
                    r["source"] = elem["source"]
                rows_all += rows
                ne = sum(r["status"] == "not-exact" for r in rows)
                print(f"[{area}] {eid} ({elem['source']}): {len(rows)} states, {ne} not exact, "
                      f"{len(elem['variants'])} variant(s) ({time.time() - t0:.1f} s)", flush=True)
                if write:
                    d = os.path.join(STATES_DIR, area if source == "approved" else area + "_work7")
                    os.makedirs(d, exist_ok=True)
                    with open(os.path.join(d, eid + ".json"), "w", encoding="utf-8", newline="\n") as f:
                        json.dump(elem, f, separators=(",", ":"))
    os.makedirs(os.path.dirname(tsv), exist_ok=True)
    with open(tsv, "w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(TSV_COLS) + "\n")
        for r in rows_all:
            f.write("\t".join(str(r.get(c, "")) for c in TSV_COLS) + "\n")
    print(f"wrote {tsv} ({len(rows_all)} rows)")
    return 0


def main_stamp(argv):
    """writes each element file's per-state scene hash (scene_sha) from the approved scenes as they are now"""
    if True:
        import port_game as PG
        PG.load_area_kinds()
    n = 0
    for d in sorted(os.listdir(STATES_DIR)):
        w7 = d.endswith("_work7")
        if w7 and "--work7" not in argv:   # (install78: round 7's pending element files are not stamped)
            continue
        area = d[:-len("_work7")] if w7 else d
        recs = area_records(area, "work7" if w7 else "approved")
        for f in sorted(os.listdir(os.path.join(STATES_DIR, d))):
            p = os.path.join(STATES_DIR, d, f)
            with open(p, encoding="utf-8") as fh:
                e = json.load(fh)
            by = {r[0]: r[4] for r in recs[e["id"]]}
            for st, ref in e["states"].items():
                ref["sha"] = scene_sha(by[st])
            with open(p, "w", encoding="utf-8", newline="\n") as fh:
                json.dump(e, fh, separators=(",", ":"))
            n += 1
    print(f"stamped {n} element files")
    return 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "derive":
        sys.exit(main_derive(sys.argv[2:]))
    if len(sys.argv) > 1 and sys.argv[1] == "stamp":
        sys.exit(main_stamp(sys.argv[2:]))
    print(__doc__)
