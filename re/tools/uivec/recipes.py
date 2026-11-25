"""uivec: the components of SimGolf's interface as parametric recipes (P41 audit, section 1). A recipe returns a scene item.

A recipe's look is a STYLE: numbers shared by every instance of the component (lengths relative to its size, colours 0..255,
and the lit-surface coefficients of sdf.py's bevel paint). re/tools/uivec/accept.py measures them on the art and writes
style.json next to this file; without that file the flat placeholders below are used.
"""
import json
import os

BUBBLE_STOPS = [0.0, 0.03, 0.07, 0.12, 0.2, 0.3, 0.45, 0.65, 0.85, 1.0]   # depth inside the body / the body's radius
RING_STOPS = [0.0, 0.04, 0.1, 0.18, 0.3, 0.45, 0.65, 0.9]            # depth inside the outer ellipse / its short radius
WELL_STOPS = [0.0, 0.05, 0.12, 0.22, 0.36, 0.55, 0.8, 1.0]           # depth inside the well / its short radius
DISC_STOPS = [0.0, 0.04, 0.09, 0.16, 0.26, 0.4, 0.6, 1.0]


# The pad's bevels have the same width in pixels on every pad, whatever its size (the big Play pad showed it: scaled
# with the pad, its ring came out banded). So the ring's and the well's depth is measured in these fixed lengths, the
# short radii of the Sandbox pad, not in each pad's own radius.
RING_L, WELL_L = 41.0, 30.4
BUBBLE_L = float(__import__("os").environ.get("UIVEC_BUBBLE_L", "0"))   # 0: the body's own radius (trial switch)


def flat(rgb, stops):
    return [[list(rgb), [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]] for _ in stops]


STYLE = {
    "bubble": {
        "body": [-0.021, -0.024, 0.938],                 # the body inside the rim (measured; the glyph is clipped to it)
        "sh": [-0.086, 0.22, 0.20, 0.42],                # the shadow cast on the pad: offset x r, softness x r, strength
        "edge": [0.0, 0.0],                              # frame pixels added to the measured rim / body radius
        "halo": [0.0, 0.03, 0.03, 0.06, 0.3],            # a dark halo: offset x r, growth x r, softness x r, strength
        "blue": {"rim": [36, 42, 90], "shade": flat([150, 170, 245], BUBBLE_STOPS), "glyph": [36, 42, 90],
                 "sp": [-0.39, -0.47, 0.27, 0.9, 2.0], "spec": [255, 255, 255]},
        "yellow": {"rim": [36, 42, 90], "shade": flat([250, 200, 20], BUBBLE_STOPS), "glyph": [36, 42, 90],
                   "sp": [-0.39, -0.47, 0.27, 0.6, 2.0], "spec": [255, 255, 255]},
    },
    "pad": {
        "idle": {"ring": flat([106, 106, 162], RING_STOPS), "well": flat([255, 255, 236], WELL_STOPS)},
        "hover": {"ring": flat([106, 106, 162], RING_STOPS), "well": flat([255, 222, 20], WELL_STOPS)},
    },
    "disc": {
        "soft": 0.6,
        "idle": {"shade": flat([104, 104, 160], DISC_STOPS), "glyph": [36, 42, 90]},
        "hover": {"shade": flat([104, 104, 160], DISC_STOPS), "glyph": [255, 218, 20]},
    },
}
_path = os.path.join(os.path.dirname(__file__), "style.json")
if os.path.exists(_path):
    with open(_path) as _f:
        for _k, _v in json.load(_f).items():
            STYLE[_k].update(_v)


def bubble(cx, cy, r, state="blue", glyph=None, shadow=None, style=None, body=None):
    """C1: a glossy sphere in a navy rim. glyph: outline rings in frame coordinates. shadow: the shape its cast shadow is
    clipped to (the pad under it). body: this instance's measured body circle (cx, cy, r), else the style's proportions."""
    S = style or STYLE["bubble"]
    p = S[state]
    if body is None:
        body = [cx + S["body"][0] * r, cy + S["body"][1] * r, S["body"][2] * r]
    bc = {"t": "circle", "c": [body[0], body[1]], "r": body[2]}
    L = []
    if shadow is not None:
        L.append({"shape": {"t": "circle", "c": [cx + S["sh"][0] * r, cy + S["sh"][1] * r], "r": r}, "fill": [30, 30, 80],
                  "alpha": S["sh"][3], "soft": max(S["sh"][2] * r, 0.3), "clip": shadow})
    h = S["halo"]
    L.append({"shape": {"t": "circle", "c": [cx + h[0] * r, cy + h[1] * r], "r": (1 + h[2]) * r}, "fill": [20, 22, 60],
              "alpha": h[4], "soft": max(h[3] * r, 0.25)})
    L.append({"shape": {"t": "circle", "c": [cx, cy], "r": r + S["edge"][0]}, "fill": p["rim"]})
    bc = {"t": "circle", "c": [body[0], body[1]], "r": body[2] + S["edge"][1]}
    L.append({"shape": bc, "fill": {"bevel": {"L": BUBBLE_L or body[2], "stops": BUBBLE_STOPS, "coef": p["shade"]}}})
    sp = p["sp"]
    L.append({"shape": bc, "fill": p["spec"], "alpha": sp[3],
              "falloff": {"c": [cx + sp[0] * r, cy + sp[1] * r], "r": max(sp[2] * r, 0.5), "pow": sp[4]}})
    if glyph:
        L.append({"shape": (glyph if isinstance(glyph, dict) else {"t": "poly", "rings": glyph}), "fill": p["glyph"], "clip": bc, "key": "glyph"})
    m = 1.3 * r
    return {"box": [cx - m, cy - m, cx + m, cy + m], "layers": L}


def disc(cx, cy, r, state="idle", glyph=None, style=None):
    """C1b: a matte raised disc (exit, back): no rim line, a bevel lit from the upper left, a soft edge."""
    S = style or STYLE["disc"]
    c = {"t": "circle", "c": [cx, cy], "r": r}
    L = [{"shape": c, "fill": {"bevel": {"L": r, "stops": DISC_STOPS, "coef": S[state]["shade"]}}, "soft": S["soft"]}]
    if glyph:
        L.append({"shape": (glyph if isinstance(glyph, dict) else {"t": "poly", "rings": glyph}), "fill": S[state]["glyph"], "clip": c, "key": "glyph"})
    m = 1.25 * r
    return {"box": [cx - m, cy - m, cx + m, cy + m], "layers": L}


def pad(outer, inner, state="idle", style=None):
    """C3: outer / inner = (cx, cy, rx, ry, rot). The ring is the outer ellipse, the well the inner one cut into it. Drawn
    over the backdrop, whose painted soft shadow under the pad is kept."""
    S = (style or STYLE["pad"])[state]
    o = {"t": "ellipse", "c": [outer[0], outer[1]], "r": [outer[2], outer[3]], "rot": outer[4]}
    i = {"t": "ellipse", "c": [inner[0], inner[1]], "r": [inner[2], inner[3]], "rot": inner[4]}
    L = [{"shape": o, "fill": {"bevel": {"L": RING_L, "stops": RING_STOPS, "coef": S["ring"]}}},
         {"shape": i, "fill": {"bevel": {"L": WELL_L, "stops": WELL_STOPS, "coef": S["well"]}}}]
    m = 6
    return {"box": [outer[0] - outer[2] - m, outer[1] - outer[3] - m, outer[0] + outer[2] + m, outer[1] + outer[3] + m],
            "layers": L}
