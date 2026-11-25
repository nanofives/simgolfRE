"""Shape helpers for hud_palettes: constructors in the style of glyphs.py and a translator (shapes and paints are authored in a
cut's LOCAL coordinates, 0..w x 0..h, and moved to each state's cut in the sheet)."""
import copy


def U(*s):
    return {"t": "union", "of": list(s)}


def line(*pts, w=1.0, cap="butt", smooth=0, closed=False):
    s = {"t": "stroke", "pts": [list(p) for p in pts], "w": w, "cap": cap}
    if smooth:
        s["smooth"] = smooth
    if closed:
        s["closed"] = True
    return s


def poly(*pts, smooth=0):
    s = {"t": "poly", "rings": [[list(p) for p in pts]]}
    if smooth:
        s["smooth"] = smooth
    return s


def polys(*rings, smooth=0):
    s = {"t": "poly", "rings": [[list(p) for p in r] for r in rings]}
    if smooth:
        s["smooth"] = smooth
    return s


def rect(x0, y0, x1, y1, r=0.0, rot=0.0):
    return {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": rot}


def rrect_c(cx, cy, hx, hy, r=0.0, rot=0.0):
    return {"t": "rrect", "c": [cx, cy], "half": [hx, hy], "r": r, "rot": rot}


def circle(cx, cy, r):
    return {"t": "circle", "c": [cx, cy], "r": r}


def oval(cx, cy, rx, ry, rot=0.0):
    return {"t": "ellipse", "c": [cx, cy], "r": [rx, ry], "rot": rot}


def seg(a, b, w):
    return {"t": "seg", "a": list(a), "b": list(b), "w": w}


def arc(cx, cy, r, a0, sweep, w):
    return {"t": "arc", "c": [cx, cy], "r": r, "a0": a0, "sweep": sweep, "w": w}


def curve(p0, p1, p2, w0, w1=None):
    return {"t": "qstroke", "p": [list(p0), list(p1), list(p2)], "w": [w0, w0 if w1 is None else w1]}


def arrow(a, b, w, hl, hw):
    return {"t": "arrow", "a": list(a), "b": list(b), "w": w, "hl": hl, "hw": hw}


def outlined(s, w):
    return {"t": "outline", "of": s, "w": w}


def sub(a, *b):
    return {"t": "sub", "a": a, "b": U(*b) if len(b) > 1 else b[0]}


def inter(*s):
    return {"t": "inter", "of": list(s)}


def half(p, n):
    return {"t": "half", "p": list(p), "n": list(n)}


def grow(s, d):
    return {"t": "grow", "of": s, "d": d}


def lathe(x, prof, smooth=0):
    return {"t": "lathe", "x": x, "prof": [list(p) for p in prof], "smooth": smooth}


def mirror_x(s, ax):
    """A shape mirrored about the vertical line x = ax."""
    s = copy.deepcopy(s)
    _mir(s, ax)
    return s


def _mir(S, ax):
    t = S.get("t")
    f = lambda p: [2 * ax - p[0], p[1]]  # noqa: E731
    if t in ("circle", "ellipse", "rrect"):
        S["c"] = f(S["c"])
        if "rot" in S:
            S["rot"] = -S["rot"]
    elif t == "arrow" or t == "seg":
        S["a"], S["b"] = f(S["a"]), f(S["b"])
    elif t == "stroke":
        S["pts"] = [f(p) for p in S["pts"]]
    elif t == "poly":
        S["rings"] = [[f(p) for p in r] for r in S["rings"]]
    elif t == "qstroke":
        S["p"] = [f(p) for p in S["p"]]
    elif t == "arc":
        S["c"] = f(S["c"])
        a0, sw = S["a0"], S["sweep"]
        S["a0"], S["sweep"] = 180 - a0 - sw, sw
    elif t == "half":
        S["p"] = f(S["p"])
        S["n"] = [-S["n"][0], S["n"][1]]
    elif t == "lathe":
        S["x"] = 2 * ax - S["x"]
    elif t == "move":
        S["d"] = [-S["d"][0], S["d"][1]]
        _mir(S["of"], ax)
    for k in ("of", "a", "b"):
        v = S.get(k)
        if isinstance(v, dict):
            _mir(v, ax)
        elif isinstance(v, list) and v and isinstance(v[0], dict):
            for q in v:
                _mir(q, ax)


def moved(obj, dx, dy):
    """A deep copy of a shape, paint, layer, item or scene with every coordinate shifted by (dx, dy)."""
    o = copy.deepcopy(obj)
    _mv(o, dx, dy)
    return o


def _pt(p, dx, dy):
    return [p[0] + dx, p[1] + dy]


def _mv(o, dx, dy):
    if isinstance(o, list):
        for q in o:
            if isinstance(q, (dict, list)):
                _mv(q, dx, dy)
        return
    if not isinstance(o, dict):
        return
    if "box" in o and "layers" in o:
        b = o["box"]
        o["box"] = [b[0] + dx, b[1] + dy, b[2] + dx, b[3] + dy]
        _mv(o["layers"], dx, dy)
        return
    if "keep_art" in o:   # a kept picture's region
        _mv(o["keep_art"], dx, dy)
        return
    if "shape" in o:      # a layer
        for k in ("shape", "clip", "fill"):
            if k in o and isinstance(o[k], dict):
                _mv(o[k], dx, dy)
        if "falloff" in o:
            o["falloff"]["c"] = _pt(o["falloff"]["c"], dx, dy)
        for f in o.get("fx", ()):
            for k in ("a", "b"):
                if k in f:
                    f[k] = _pt(f[k], dx, dy)
        return
    if "linear" in o:
        l = o["linear"]
        l["a"], l["b"] = _pt(l["a"], dx, dy), _pt(l["b"], dx, dy)
        return
    if "radial" in o:
        o["radial"]["c"] = _pt(o["radial"]["c"], dx, dy)
        return
    if "bevel" in o:
        if "of" in o["bevel"]:
            _mv(o["bevel"]["of"], dx, dy)
        return
    t = o.get("t")
    if t in ("circle", "ellipse", "rrect", "arc", "dimpled"):
        o["c"] = _pt(o["c"], dx, dy)
    elif t in ("arrow", "seg"):
        o["a"], o["b"] = _pt(o["a"], dx, dy), _pt(o["b"], dx, dy)
    elif t == "stroke":
        o["pts"] = [_pt(p, dx, dy) for p in o["pts"]]
    elif t == "poly":
        o["rings"] = [[_pt(p, dx, dy) for p in r] for r in o["rings"]]
    elif t == "qstroke":
        o["p"] = [_pt(p, dx, dy) for p in o["p"]]
    elif t == "half":
        o["p"] = _pt(o["p"], dx, dy)
    elif t == "lathe":
        o["x"] += dx
        o["prof"] = [[p[0] + dy, p[1]] for p in o["prof"]]
    elif t in ("union", "inter"):
        _mv(o["of"], dx, dy)
    elif t == "sub":
        _mv(o["a"], dx, dy)
        _mv(o["b"], dx, dy)
    elif t in ("grow", "outline"):
        _mv(o["of"], dx, dy)
    elif t == "move":
        _mv(o["of"], dx, dy)
    elif t is not None and "." in t:      # an area shape: its module moves it
        import shapes_ext
        shapes_ext.move(o, dx, dy)


def rbox(x0, y0, x1, y1, rl, rr, n=10):
    """A box whose left corners have radius rl and right corners radius rr, as a polygon (arcs sampled n per corner)."""
    import math
    pts = []
    for cx, cy, r, a0 in ((x1 - rr, y0 + rr, rr, -90), (x1 - rr, y1 - rr, rr, 0), (x0 + rl, y1 - rl, rl, 90), (x0 + rl, y0 + rl, rl, 180)):
        for k in range(n + 1):
            a = math.radians(a0 + 90.0 * k / n)
            pts.append([round(cx + r * math.cos(a), 3), round(cy + r * math.sin(a), 3)])
    return {"t": "poly", "rings": [pts]}


def ebox(x0, y0, x1, y1, lx, ly, rx, ry, n=12):
    """A box with elliptical corners: left corners lx by ly, right corners rx by ry, as a polygon."""
    import math
    pts = []
    for cx, cy, ax, ay, a0 in ((x1 - rx, y0 + ry, rx, ry, -90), (x1 - rx, y1 - ry, rx, ry, 0), (x0 + lx, y1 - ly, lx, ly, 90),
                               (x0 + lx, y0 + ly, lx, ly, 180)):
        for k in range(n + 1):
            a = math.radians(a0 + 90.0 * k / n)
            pts.append([round(cx + ax * math.cos(a), 3), round(cy + ay * math.sin(a), 3)])
    return {"t": "poly", "rings": [pts]}
